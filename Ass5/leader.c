#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

int main(int argc, char *argv[])
{

    int n = 10;  
    if (argc > 1) 
    {
        n = atoi(argv[1]);
        if (n > 100) 
        {
            fprintf(stderr, "Error: n cannot exceed 100\n");
            exit(EXIT_FAILURE);
        }
    }

	int shmid;
    key_t key = ftok(".",'P');
	shmid = shmget(key, 104*sizeof(int), 0777|IPC_CREAT|IPC_EXCL);

    if(shmid==-1)
    {
        printf("Already A Leader Is Running.\n");
        exit(EXIT_FAILURE);
    }

	int *M  = (int *) shmat(shmid, 0, 0);

    if(M==(int*)-1)
    {
        printf("Error Attaching shm To Leader in Leader\n");
        exit(EXIT_FAILURE);
    }

    M[0]=n;
    M[1]=0;
    M[2]=0;

    while(M[1]<n)
    {
        sleep(1);
    }
    srand(time(NULL));
    bool *hash_table = (bool*) malloc(1000*sizeof(bool));

    int sum;

    while (1) 
    {
        M[3] = 1+rand()%99;
        M[2] = 1;

        while (M[2] != 0) continue;

        sum = M[3];
        printf("%2d", M[3]);
        for (int i = 4; i <= n + 3; i++) 
        {
            sum += M[i];
            printf(" + %d", M[i]);
        }
        printf(" = %3d\n", sum);

        if (hash_table[sum]) 
        {
            M[2] = -1;
            break;
        } 
        hash_table[sum] = 1;
    }

    while (M[2] != 0) continue;

    shmdt(M);
    shmctl(shmid, IPC_RMID, 0);
    free(hash_table);

    return 0;
}

