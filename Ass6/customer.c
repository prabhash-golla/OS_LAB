#include<stdio.h>
#include<sys/sem.h>
#include<sys/shm.h>
#include<sys/ipc.h>
#include<time.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>

int Shm, Sem;
int * M;
#define P(s, i) semop(s, &(struct sembuf){i, -1, 0}, 1)
#define V(s, i) semop(s, &(struct sembuf){i, 1, 0}, 1)


void cmain(int customerId,int arrivalTime,int numberofPersons)
{
    //------- Check for Late Arrival ------- //
    if(arrivalTime>240)
    {
        printf("[%02d:%02d %cm] \t\t\t\t\t\tCustomer %d leaves (late arrival)\n",(10+arrivalTime/60)%12+1,arrivalTime%60,(arrivalTime/60)? 'p':'a',customerId);
        fflush(stdout);
        exit(EXIT_SUCCESS);
    }
    P(Sem,0);
    //------- Check for Empty Table ------- //
    if(M[1])
    {
        // ------- Write in Waiter Queue & Wake him up  ------- //
        if(M[0]<=arrivalTime) M[0] = arrivalTime;
        else
        {
            perror("Time Error");
            V(Sem, 0);
            exit(EXIT_FAILURE);
        }
        printf("[%02d:%02d %cm] Customer %d arrives (count = %d)\n",(10+arrivalTime/60)%12+1,arrivalTime%60,(arrivalTime/60)? 'p':'a',customerId,numberofPersons);
        fflush(stdout);
        M[1]--;
        int waiterId = M[2];
        M[2] = (M[2]+1)%5;
        //------- BW is the Pointer to End of the Waiter Queue ------- //
        int BW = M[103+waiterId*200]++;
        M[104+waiterId*200+2*BW] = customerId;
        M[105+waiterId*200+2*BW] = numberofPersons;
        M[101+waiterId*200]++;
        V(Sem,0);

        // ------- Wake Up the Waiter ------- //
        V(Sem, 2+waiterId);

        // ------- Wait for Order to be Placed ------- //
        P(Sem, 6+customerId);

        // ------- Order Placed ------- //
        P(Sem,0);
        printf("[%02d:%02d %cm] \tCustomer %d: Order placed to Waiter %c\n",(10+(M[0])/60)%12+1,(M[0])%60,(M[0]/60)? 'p':'a',customerId,'U'+waiterId);
        fflush(stdout);
        V(Sem, 0);

        // ------- Wait for Food to be Served ------- //
        P(Sem,6+customerId);

        // ------- Food Served ------- //
        P(Sem,0);
        int time1 = M[0];
        printf("[%02d:%02d %cm] \t\tCustomer %d gets food [Waiting time = %d]\n",(10+time1/60)%12+1,(time1)%60,(time1/60)? 'p':'a',customerId,time1-arrivalTime);
        fflush(stdout);
        V(Sem,0);

        // ------- Eat Food ------- //
        usleep(30*100000);

        // ------- Finish Eating ------- //
        P(Sem,0);
        M[1]++;
        if(M[0]<=time1+30) M[0] = time1+30;
        else
        {
            perror("Time Error");
            V(Sem, 0);
            exit(EXIT_FAILURE);
        }
        printf("[%02d:%02d %cm] \t\t\tCustomer %d finishes eating and leaves\n",(10+M[0]/60)%12+1,M[0]%60,(M[0]/60)? 'p':'a',customerId);
        fflush(stdout);
        V(Sem,0);
    }
    else
    {
        // ------- No Empty Table ------- //
        printf("[%02d:%02d %cm] \t\t\t\t\t\tCustomer %d leaves (no empty table)\n",(10+arrivalTime/60)%12+1,arrivalTime%60,(arrivalTime/60)? 'p':'a',customerId);
        fflush(stdout);
        V(Sem,0);
    }
    shmdt(M);
    exit(EXIT_SUCCESS);
}

int main()
{
    //------- Get Shared Memory & Semaphore ------- //
    key_t shm_key = ftok(".", 'P');
    key_t sem_key = ftok(".", 'Q');
    Shm = shmget(shm_key, 2000 * sizeof(int), 0777);
    Sem = semget(sem_key, 207, 0777);
    if (Shm == -1 || Sem == -1) 
    {
        perror("shmget() or semget() failed");
        exit(EXIT_FAILURE);
    }

    //------- Attach to Shared Memory ------- //
    M = (int *)shmat(Shm, NULL, 0);
    if (M == (int *)-1) 
    {
        perror("shmat() failed");
        exit(EXIT_FAILURE);
    }

    //------- Read from File ------- //
    FILE* fptr = fopen("customers.txt","r");

    if(!fptr)
    {
        perror("fopen() failed");
        exit(EXIT_FAILURE);
    }

    int ID,arrivalTime,count,lastArrivalTime=0,lastID=0;

    while(fscanf(fptr,"%d %d %d",&ID,&arrivalTime,&count)==3)
    {
        if(ID==-1) break;
        //------- Sleep till time becomes Arrival Time ------- //
        usleep((arrivalTime-lastArrivalTime)*100000);
        pid_t pid = fork();
        if(pid==-1)
        {
            perror("fork() failed");
            exit(EXIT_FAILURE);
        }
        if(pid==0) cmain(ID,arrivalTime,count);
        lastArrivalTime = arrivalTime;
        lastID = ID;
    }
    fclose(fptr);

    //------- Wait for Customers to Leave ------- //
    for(int i=0;i<lastID;i++) wait(NULL);

    // ------- Clean Up ------- //
    shmdt(M);
    shmctl(Shm, IPC_RMID, NULL);
    semctl(Sem, 0, IPC_RMID);

    return 0;
}
