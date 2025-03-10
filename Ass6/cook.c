#include<stdio.h>
#include<sys/sem.h>
#include<sys/shm.h>
#include<sys/ipc.h>
#include<sys/wait.h>
#include<signal.h>
#include<time.h>
#include<stdlib.h>
#include<unistd.h>

#define P(s, i) semop(s, &(struct sembuf){i, -1, 0}, 1)
#define V(s, i) semop(s, &(struct sembuf){i, 1, 0}, 1)

int Shm, Sem;
int * M,j=1;

void sig(int sig)
{
    printf("Cooks received shutdown signal\n");
    shmctl(Shm, IPC_RMID, NULL);
    semctl(Sem, 0, IPC_RMID);
    exit(EXIT_SUCCESS);
}

void cmain(int i)
{
    signal(SIGINT, sig);
    
    // ------- Cook's Initialization ------- //
    printf("[11:00 am] %sCook %c is ready\n",i? "\t":"",'C'+i);
    fflush(stdout);
    while (1)
    {
        // ------- Wait till a Order Comes ------- //
        P(Sem, 1);

        // ------- Accepting Order From Cook Queue ------- //
        P(Sem, 0);
        // ------- F is the Pointer to Start of the Cook Queue ------- //
        int F = M[1100]++;
        int waiterNo = M[1102+F*3];
        int customerId = M[1103+F*3];
        int numberofPersons = M[1104+F*3];
        int sleepTime = numberofPersons*5;
        int presentTime = M[0];
        int finishTime = presentTime+sleepTime;
        M[3]--;
        printf("[%02d:%02d %cm] %sCook %c: Preparing order (Waiter %c, Customer %d, Count %d)\n",(10+presentTime/60)%12+1,presentTime%60,(presentTime/60)? 'p':'a',i? "\t":"",'C'+i,'U'+waiterNo,customerId,numberofPersons);
        fflush(stdout);
        V(Sem, 0);

        // ------- Sleep While Cook is Cooking ------- //
        usleep(sleepTime*100000);

        // ------- Order Preparation done give it to waiter ------- //
        P(Sem, 0);
        if(M[0]<=finishTime) M[0] = finishTime;
        else
        {
            perror("Time Error");
            V(Sem, 0);
            exit(EXIT_FAILURE);
        }
        M[100+waiterNo*200] = customerId;
        printf("[%02d:%02d %cm] %sCook %c: Prepared order (Waiter %c, Customer %d, Count %d)\n",(10+finishTime/60)%12+1,finishTime%60,(finishTime/60)? 'p':'a',i? "\t":"",'C'+i,'U'+waiterNo,customerId,numberofPersons);
        fflush(stdout);
        V(Sem, 0);

        // ------- Wake Up the Waiter ------- //
        V(Sem, 2+waiterNo);

        // ------- Check For Restaurent Closing ------- //
        P(Sem, 0);
        if(M[0]>=240 && !M[3])
        {
            printf("[%02d:%02d %cm] %sCook %c: Leaving\n",(10+M[0]/60)%12+1,M[0]%60,(M[0]/60)? 'p':'a',i? "\t":"",'C'+i);
            fflush(stdout);
            P(Sem,207);
            if(j==1) j=0;
            else 
            {
                for(int i=0;i<5;i++)
                {
                    V(Sem, 2+i);
                }
            }
            V(Sem,207);
            V(Sem, 0);
            shmdt(M);
            exit(EXIT_SUCCESS);
        }
        V(Sem, 0);
    }
}

int main()
{
    // ------- Shared Memory & Semaphore Creation ------- //
    key_t shm_key = ftok(".", 'P');
    key_t sem_key = ftok(".", 'Q');
    pid_t pid_C,pid_D;

    Shm = shmget(shm_key, 2000 * sizeof(int), IPC_CREAT | 0777 | IPC_EXCL);
    Sem = semget(sem_key, 208, IPC_CREAT | 0777 | IPC_EXCL);
    
    if (Shm == -1 || Sem == -1) 
    {
        perror("Failed to create shared resources");
        exit(EXIT_FAILURE);
    }
    
    // ------- Shared Memory Attachment ------- //
    M = shmat(Shm, NULL, 0);
    if (M == (int *)-1) 
    {
        perror("shmat() failed");
        exit(EXIT_FAILURE);
    }

    // ------- Initialization ------- //
    M[0] = 0;        // ------- time ------- //
    M[1] = 10;       // ------- number of empty tables -------//
    M[2] = 0;        // ------- waiter number to serve the next customer -------//
    M[3] = 0;        // ------- number of orders pending for the cooks -------//

    // ------- Semophores ------- //
    /*
        +-------+--------+
        |Index-i|Protects|
        +-------+--------+
        |   0   |    M   |
        +-------+--------+
        |   1   |  Cook  |
        +-------+--------+
        |  2-6  |Waiter-i|
        +-------+--------+
        | 7-206 |Customer|
        +-------+--------+
        |  207  |    j   |
        +-------+--------+
    */
    semctl(Sem, 0, SETVAL, 1);
    for(int i=1;i<207;i++) semctl(Sem, i, SETVAL, 0);
    semctl(Sem, 207, SETVAL, 1);

    // ------- Cook's Creation ------- //
    if(!(pid_C=fork())) cmain(0);
    if(!(pid_D=fork())) cmain(1);

    // ------- Wait for Cook's to Finish ------- //
    wait(NULL);
    wait(NULL);

    // ------- Clean Up ------- //
    shmdt(M);
    shmctl(Shm, IPC_RMID, NULL);
    semctl(Sem, 0, IPC_RMID);

    return 0;
}