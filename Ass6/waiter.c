#include<stdio.h>
#include<sys/sem.h>
#include<sys/shm.h>
#include<sys/ipc.h>
#include<sys/wait.h>
#include<signal.h>
#include<time.h>
#include<stdlib.h>
#include<unistd.h>

#define NUMBER_OF_WAITERS 5
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

void wmain(int waiterNo)
{
    signal(SIGINT, sig);
    printf("[11:00 am] Waiter %c is ready\n",'U'+waiterNo);
    while (1)
    {
        P(Sem, 2+waiterNo);
        // printf("Waiter %c\n",'U'+waiterNo);
        P(Sem, 0);
        if(M[100+waiterNo*200])
        {
            int presentTime = M[0];
            int customerId = M[100+waiterNo*200];
            printf("[%2d:%2d] Waiter %c: Serving food to Customer %d\n",(10+presentTime/60)%12+1,presentTime%60,'U'+waiterNo,customerId);
            M[100+waiterNo*200] = 0;
            V(Sem, 0);
            V(Sem, 6+customerId);
        }
        else if(M[101+waiterNo*200])
        {
            M[101+waiterNo*200]--;
            int FW = M[102+waiterNo*200]++;
            int B = M[1101]++;
            M[1102+B*3] = waiterNo;
            M[1103+B*3] = M[104+waiterNo*200+FW*2];
            M[1104+B*3] = M[105+waiterNo*200+FW*2];
            M[3]++;
            printf("[%2d:%2d] Waiter %c: Placing order for Customer %d (count = %d)\n",(10+M[0]/60)%12+1,M[0]%60,'U'+waiterNo,M[104+waiterNo*200+FW*2],M[105+waiterNo*200+FW*2]);
            V(Sem, 1);
        }
        V(Sem,0);
    }
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

    for(int i=0;i<NUMBER_OF_WAITERS;i++)
    {
        pid_t pid = fork();
        if(pid==0)
        {
            wmain(i);
        }
    }

    for(int i=0;i<NUMBER_OF_WAITERS;i++)
    {
        wait(NULL);
    }

    shmdt(M);

    return 0;
}
