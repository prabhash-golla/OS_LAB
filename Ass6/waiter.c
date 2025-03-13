#include<stdio.h>
#include<sys/sem.h>
#include<sys/shm.h>
#include<sys/ipc.h>
#include<sys/wait.h>
#include<signal.h>
#include<time.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>

#define NUMBER_OF_WAITERS 5
#define P(s, i) semop(s, &(struct sembuf){i, -1, 0}, 1)
#define V(s, i) semop(s, &(struct sembuf){i, 1, 0}, 1)

int Shm, Sem;
int * M;

// ------- Generate String of i Characters with fillChar ------- //
char* generateString(int i, char fillChar) 
{
    char *str = (char*)malloc((i + 1) * sizeof(char));
    if (str == NULL) 
    {
        printf("Memory allocation failed\n");
        return NULL;
    }
    
    memset(str, fillChar, i);
    str[i] = '\0';           
    
    return str;
}

void wmain(int waiterNo)
{
    char* space = generateString(waiterNo, '\t');
    printf("[11:00 am] %sWaiter %c is ready\n",space,'U'+waiterNo);
    while (1)
    {
        // ------- Wait for A Customer or Cook to Wakeup ------- //
        P(Sem, 2+waiterNo);

        P(Sem, 0);

        // ------- Serve order to Customer (Woken Up by Cook) ------- //
        if(M[100+waiterNo*200])
        {
            // ------- Serve the Food ------- //
            int presentTime = M[0];
            int customerId = M[100+waiterNo*200];
            printf("[%02d:%02d %cm] %sWaiter %c: Serving food to Customer %d\n",(10+presentTime/60)%12+1,presentTime%60,(presentTime/60)? 'p':'a',space,'U'+waiterNo,customerId);
            fflush(stdout);
            M[100+waiterNo*200] = 0;
            if(M[0]>=240 && M[102+waiterNo*200]==M[103+waiterNo*200])
            {
                // ------- Check For Restaurent Closing ------- //
                printf("[%02d:%02d %cm] %sWaiter %c leaving (no more customer to serve)\n",(10+M[0]/60)%12+1,M[0]%60,(M[0]/60)? 'p':'a',space,'U'+waiterNo);
                fflush(stdout);
                V(Sem, 0);
                V(Sem, 6+customerId);
                if (space != NULL) 
                {
                    free(space);
                    space = NULL;
                }
                if (M != (int*)-1) 
                {
                    shmdt(M);
                    M = (int*)-1;
                }
                exit(EXIT_SUCCESS);
            }
            V(Sem, 0);
            V(Sem, 6+customerId);
        }
        else
        // ------- Take order from Customer (Woken Up by Customer) ------- //
        if(M[101+waiterNo*200])
        {

            // ------- Write in Cook Queue ------- //
            M[101+waiterNo*200]--;
            //------- FW is the Pointer to Start of the Waiter Queue ------- //
            int FW = M[102+waiterNo*200]++;
            int customerId = M[104+waiterNo*200+FW*2];
            int count = M[105+waiterNo*200+FW*2];
            //------- B is the Pointer to End of the Cook Queue ------- //
            int B = M[1101]++;  
            M[1102+B*3] = waiterNo;
            M[1103+B*3] = customerId;
            M[1104+B*3] = count;
            M[3]++;
            int presentTime = M[0];
            V(Sem,0);
            
            // ------- For Taking the Order From Customer ------- //
            usleep(100000);
            
            // ------- Order Taken ------- //
            printf("[%02d:%02d %cm] %sWaiter %c: Placing order for Customer %d (count = %d)\n",(10+(presentTime+1)/60)%12+1,(presentTime+1)%60,((presentTime+1)/60)? 'p':'a',space,'U'+waiterNo,customerId,count);
            fflush(stdout);
            P(Sem,0);
            if(M[0] <= presentTime+1) M[0] = presentTime+1;
            else
            {
                perror("Time Error");
                V(Sem, 0);
                shmdt(M);
                exit(EXIT_FAILURE);
            }
            V(Sem,0);

            // ------- Wake Up the Customer ( Notify Order is Placed ) ------- //
            V(Sem, 6+customerId);

            // ------- Wake Up the Cook ------- //
            V(Sem,1);
        }
        else
        // ------- Check For Restaurent Closing ------- // 
        if(M[0]>=240 && M[102+waiterNo*200]==M[103+waiterNo*200])
        {
            printf("[%02d:%02d %cm] %sWaiter %c leaving (no more customer to serve)\n",(10+M[0]/60)%12+1,M[0]%60,(M[0]/60)? 'p':'a',space,'U'+waiterNo);
            fflush(stdout);
            V(Sem, 0);
            free(space);
            shmdt(M);
            exit(EXIT_SUCCESS);
        }
    }
}

int main()
{
    // ------- Get Shared Memory & Semaphore ------- //
    key_t shm_key = ftok(".", 'P');
    key_t sem_key = ftok(".", 'Q');
    Shm = shmget(shm_key, 2000 * sizeof(int), 0777);
    Sem = semget(sem_key, 207, 0777);
    if (Shm == -1 || Sem == -1) 
    {
        perror("shmget() or semget() failed");
        exit(EXIT_FAILURE);
    }

    // ------- Attach to Shared Memory ------- //
    M = (int *)shmat(Shm, NULL, 0);
    if (M == (int *)-1) 
    {
        perror("shmat() failed");
        exit(EXIT_FAILURE);
    }

    // ------- Create Waiters ------- //
    for(int i=0;i<NUMBER_OF_WAITERS;i++)
    {
        pid_t pid = fork();
        if(pid == -1)
        {
            perror("fork() failed");
            exit(EXIT_FAILURE);
        }
        if(!pid) wmain(i);
    }

    // ------- Wait for Waiters to Finish ------- //
    for(int i=0;i<NUMBER_OF_WAITERS;i++) wait(NULL);

    // ------- Detach from Shared Memory ------- //
    shmdt(M);

    return 0;
}
