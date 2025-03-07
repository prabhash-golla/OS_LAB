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
int * M;

void sig(int sig)
{
    shmctl(Shm, IPC_RMID, NULL);
    semctl(Sem, 0, IPC_RMID);
    exit(EXIT_SUCCESS);
}

void cmain(int i)
{
    signal(SIGINT, sig);
    printf("[11:00 am] Cook %c is ready\n",'C'+i);
    while (1)
    {
        P(Sem, 1);
        P(Sem, 0);
        if(M[0]>240) break;
        if(!M[3]) continue;
        int F = M[1100];
        int waiterNo = M[1102+F*3];
        int customerId = M[1103+F*3];
        int numberofPersons = M[1104+F*3];
        M[1100] = (F+1);
        int sleepTime = numberofPersons*5;
        int presentTime = M[0];
        int finishTime = presentTime+sleepTime;
        V(Sem, 0);
        printf("[%2d:%2d] Cook %c: Preparing order (Waiter %c, Customer %d, Count %d)\n",(10+presentTime/60)%12+1,presentTime%60,'C'+i,'U'+waiterNo,customerId,numberofPersons);
        usleep(sleepTime*100000);
        P(Sem, 0);
        if(M[0]<=finishTime)
        {
            M[0] = finishTime;
        }
        else
        {
            perror("Time Error");
            V(Sem, 0);
            exit(EXIT_FAILURE);
        }
        M[100+waiterNo*200] = customerId;
        printf("[%2d:%2d] Cook %c: Prepared order (Waiter %c, Customer %d, Count %d)\n",(10+finishTime/60)%12+1,finishTime%60,'C'+i,'U'+waiterNo,customerId,numberofPersons);
        V(Sem, 0);
        V(Sem, 2+waiterNo);
    }
}

int main()
{
    key_t shm_key = ftok(".", 'P');
    key_t sem_key = ftok(".", 'Q');
    pid_t pid_C,pid_D;

    Shm = shmget(shm_key, 2000 * sizeof(int), IPC_CREAT | 0777 | IPC_EXCL);
    Sem = semget(sem_key, 207, IPC_CREAT | 0777 | IPC_EXCL);
    
    if (Shm == -1 || Sem == -1) 
    {
        perror("Failed to create shared resources");
        exit(EXIT_FAILURE);
    }
    
    M = shmat(Shm, NULL, 0);
    if (M == (int *)-1) 
    {
        perror("shmat() failed");
        exit(EXIT_FAILURE);
    }

    M[0] = 0;
    M[1] = 10;
    M[2] = 0;
    M[3] = 0;  

    semctl(Sem, 0, SETVAL, 1);
    for(int i=1;i<207;i++)
    {
        semctl(Sem, i, SETVAL, 0);
    }

    if(!(pid_C=fork())) cmain(0);
    if(!(pid_D=fork())) cmain(1);

    wait(NULL);
    wait(NULL);

    shmctl(Shm, IPC_RMID, NULL);
    semctl(Sem, 0, IPC_RMID);

    return 0;
}