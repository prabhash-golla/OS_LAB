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
    if(arrivalTime>240)
    {
        exit(EXIT_SUCCESS);
    }
    P(Sem,0);
    // printf("M[1] : %d\n",M[1]);
    if(M[1])
    {
        printf("[%d:%d] Customer %d arrives (count = %d)\n",(10+arrivalTime/60)%12+1,arrivalTime%60,customerId,numberofPersons);
        M[1]--;
        V(Sem,0);
        usleep(100000);
        P(Sem,0);
        if(M[0]<=arrivalTime+1)
        {
            M[0] = arrivalTime+1;
        }
        else
        {
            perror("Time Error");
            V(Sem, 0);
            exit(EXIT_FAILURE);
        }
        int waiterId = M[2];
        printf("[%2d:%2d] Customer %d: Order placed to Waiter %c\n",(10+(arrivalTime+1)/60)%12+1,(arrivalTime+1)%60,customerId,'U'+waiterId);
        int FW = M[102+waiterId*200];
        M[104+waiterId*200+2*FW] = customerId;
        M[105+waiterId*200+2*FW] = numberofPersons;
        M[101+waiterId*200]++;
        M[2] = (M[2]+1)%5;
        V(Sem, 2+waiterId);
        V(Sem, 0);

        P(Sem,6+customerId);
        P(Sem,0);
        int time1 = ++M[0];
        printf("[%2d:%2d] Customer %d gets food [Waiting time = %d]\n",(10+time1/60)%12+1,(time1+1)%60,customerId,M[0]-arrivalTime);
        V(Sem,0);
        usleep(30*100000);

        P(Sem,0);
        if(M[0]<=time1+30)
        {
            M[0] = time1+30;
        }
        else
        {
            perror("Time Error");
            V(Sem, 0);
            exit(EXIT_FAILURE);
        }
        printf("[%2d:%2d] Customer %d leaves\n",(10+M[0]/60)%12+1,M[0]%60,customerId);
        V(Sem,0);
    }
    else
    {
        printf("[%d:%d] Customer %d leaves (no empty table)\n",(10+arrivalTime/60)%12+1,arrivalTime%60,customerId);
    }
    exit(EXIT_SUCCESS);
}

int main()
{
    key_t shm_key = ftok(".", 'P');
    key_t sem_key = ftok(".", 'Q');
    Shm = shmget(shm_key, 2000 * sizeof(int), 0777);
    Sem = semget(sem_key, 7, 0777);
    if (Shm == -1 || Sem == -1) 
    {
        perror("shmget() or semget() failed");
        exit(EXIT_FAILURE);
    }

    M = (int *)shmat(Shm, NULL, 0);
    if (M == (int *)-1) 
    {
        perror("shmat() failed");
        exit(EXIT_FAILURE);
    }

    FILE* fptr = fopen("customers.txt","r");

    if(!fptr)
    {
        perror("fopen() failed");
        exit(EXIT_FAILURE);
    }

    int ID,arrivalTime,count,lastArrivalTime=0;

    while(fscanf(fptr,"%d %d %d",&ID,&arrivalTime,&count)!=EOF)
    {
        if(ID==-1)
        {
            break;
        }
        usleep((arrivalTime-lastArrivalTime)*100000);
        pid_t pid = fork();
        if(pid==0)
        {
            cmain(ID,arrivalTime,count);
        }
        lastArrivalTime = arrivalTime;
    }

    fclose(fptr);

    return 0;
}
