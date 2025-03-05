#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

void follower_process(int *M, int id) 
{
    printf("follower %d joins\n", id);
    srand(time(NULL) ^ (id << 16));
    while (1) 
    {
        if (M[2] == -id) 
        {
            M[2] = -((id+1)%(M[0]+1));
            break;
        }
        if(M[2]==id)
        {
            M[3 + id] = 1+rand()%9;
            M[2] = (id+1)%(M[0]+1);
        }
        // printf("M[2] : %d\n",M[2]);
    }
    printf("follower %d leaves\n", id);
}

int main(int argc, char *argv[]) {
    int nf = 1; 
    if (argc > 1) 
    {
        nf = atoi(argv[1]);
    }

    key_t key = ftok(".", 'P');
    int shmid = shmget(key, 104 * sizeof(int), 0777);
    if (shmid == -1) 
    {
        printf("Error No Leader Found Yet\n");
        exit(EXIT_FAILURE);
    }
    int *M = (int *)shmat(shmid, NULL, 0);
    if (M == (int *)-1) 
    {
        printf("Error Attaching shm To Leader in Leader\n");
        exit(EXIT_FAILURE);
    }
    int joined = 0;
    for (int i = 0; i < nf; i++) 
    {
        pid_t pid = fork();
        if (pid == 0) 
        {
            int id;
            if (M[1] < M[0]) 
            {
                id = ++M[1];
                joined++;
            } 
            else 
            {
                printf("follower error: %d followers have already joined\n", M[0]);
                shmdt(M);
                exit(EXIT_FAILURE);
            }
            follower_process(M, id);
            shmdt(M);
            exit(EXIT_SUCCESS);
        }
    }
    for (int i = 0; i < nf; i++) 
    {
        wait(NULL);
    }

    shmdt(M);
    return 0;
}
