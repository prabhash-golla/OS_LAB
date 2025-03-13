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
#include<pthread.h>

// ------- Structure For Semophores ------- //
typedef struct 
{
    int value;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} semaphore;

// ------- Wait Function ------- //
void P(semaphore *s) 
{
    pthread_mutex_lock(&s->mtx);
    s->value--;
    if (s->value < 0) pthread_cond_wait(&s->cv, &s->mtx);
    pthread_mutex_unlock(&s->mtx);
}

// ------- Signal Function ------- //
void V(semaphore *s) 
{
    pthread_mutex_lock(&s->mtx);
    s->value++;
    if (s->value <= 0) pthread_cond_signal(&s->cv);
    pthread_mutex_unlock(&s->mtx);
}

// ------- Global Variables and Semopheres ------- //
int m,n;
semaphore boat = {0,PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER};
semaphore rider = {0,PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER};

pthread_mutex_t bmtx = PTHREAD_MUTEX_INITIALIZER;
int *BA;  
int *BC;  
int *BT;  
pthread_barrier_t *BB;  
pthread_barrier_t EOS; 

int random_time(int min, int max) 
{
    return min + rand() % (max - min + 1);
}

// ------- Function for Boat Thread ------- //
void *Boat(void * arg)
{
    int boatId = *((int*)arg);
    free(arg);

    printf("Boat \t%3d\tReady\n",boatId);

    // ------- Initilize barrier ------- //
    pthread_barrier_init(&BB[boatId-1], NULL, 2);

    // ------- Make Boat Ready ------- //
    pthread_mutex_lock(&bmtx);
    BA[boatId-1] = 1;
    BC[boatId-1] = -1;
    pthread_mutex_unlock(&bmtx);

    while (1)
    {
        V(&rider);
        P(&boat);

        // ------- Exit Condition ------- //
        pthread_mutex_lock(&bmtx);
        if(n==0)
        {
            V(&boat);
            pthread_mutex_unlock(&bmtx);
            exit(EXIT_SUCCESS);
        }
        pthread_mutex_unlock(&bmtx);

        // ------- Wait on barrier ------- //
        pthread_barrier_wait(&BB[boatId-1]);

        // ------- Get rtime and riderId ------- //
        pthread_mutex_lock(&bmtx);
        int riderId = BC[boatId-1];
        int rtime = BT[boatId-1];
        pthread_mutex_unlock(&bmtx);

        printf("Boat \t%3d\tStart of ride for Visitor %3d  (ride time = %2d)\n",boatId,riderId,rtime);

        // ------- Sleep for rtime ------- //
        usleep(rtime*100000);

        printf("Boat \t%3d\tEnd of ride for Visitor %3d (ride time = %2d)\n",boatId,riderId,rtime);

        // ------- Set Boat Free ------- //
        pthread_mutex_lock(&bmtx);
        BA[boatId-1] = 1;
        BC[boatId-1] = -1;
        n--;
        if(n==0)
        {
            pthread_mutex_unlock(&bmtx);
            pthread_barrier_wait(&EOS);
            V(&boat);
            exit(EXIT_SUCCESS);
        } 
        pthread_mutex_unlock(&bmtx);
    }
}

// ------- Function for Rider Thread ------- //
void *Rider(void* arg)
{
    int riderId = *((int*)arg);
    free(arg);

    // ------- Generate vtime & rtime ------- //
    int vtime = random_time(30,120);
    int rtime = random_time(15, 60);

    printf("Visitor\t%3d\tStarts sightseeing for %3d minutes\n",riderId,vtime);

    // ------- Sleep for vtime ------- //
    usleep(vtime*100000);

    printf("Visitor\t%3d\tReady to ride a boat (ride time = %2d)\n",riderId,rtime);

    V(&boat);
    P(&rider);

    // ------- Find a Empty Boat ------- //
    int boat = -1;
    while (boat==-1)
    {
        pthread_mutex_lock(&bmtx);
        for(int j=0;j<m;j++)
        {
            if (BA[j] == 1 && BC[j] == -1) 
            {
                
                BC[j] = riderId;
                BT[j] = rtime;
                boat = j + 1;
                break;
            }
        }
        pthread_mutex_unlock(&bmtx);
        
        if(boat == -1)
        {
            usleep(100000);
        }
    }

    printf("Visitor\t%3d\tFinds boat %2d\n", riderId, boat);

    // ------- Wait on Boat Barrier ------- //
    pthread_barrier_wait(&BB[boat-1]);

    // ------- Sleep for rtime ------- //
    usleep(rtime*100000);

    printf("Visitor\t%3d\tLeaving\n",riderId);

}

int main(int argc,char* argv[])
{
    if(argc!=3)
    {
        printf("The Input should conatain : %s <m> & <n>\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // ------- Read m & n ------- //
    m = atoi(argv[1]);
    n = atoi(argv[2]);
    int k = n;

    if( n<20 | n>100)
    {
        printf("<n> Out of Range\n");
        exit(EXIT_FAILURE);
    }
    
    if( m<5 | m>10)
    {
        printf("<m> Out of Range\n");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));
    
    // ------- Shared Memory Creation ------- //
    BA = (int*)calloc(m, sizeof(int));  
    BB = (pthread_barrier_t*)malloc(m * sizeof(pthread_barrier_t));
    BC = (int*)malloc(m * sizeof(int));
    BT = (int*)malloc(m * sizeof(int));

    // ------- EOS Barrier : Initilisation ------- //
    pthread_barrier_init(&EOS, NULL, 2);

    // ------- Creating Boat Threads ------- //
    pthread_t *boat_threads = (pthread_t*)malloc(m * sizeof(pthread_t));
    for (int i = 0; i < m; i++) 
    {
        int *boadId = (int*)malloc(sizeof(int));
        *boadId = i + 1;  
        pthread_create(&boat_threads[i], NULL, Boat, boadId);
    }
    
    // ------- Creating Rider Threads ------- //
    pthread_t *rider_threads = (pthread_t*)malloc(n * sizeof(pthread_t));
    for (int i = 0; i < n; i++) 
    {
        int *riderId = (int*)malloc(sizeof(int));
        *riderId = i + 1;  
        pthread_create(&rider_threads[i], NULL, Rider, riderId);
    }

    // ------- Wait All Rider Threads Join ------- //
    for (int i = 0; i < k; i++) pthread_join(rider_threads[i], NULL);
    
    // ------- Wait on EOS Barrier ------- //
    pthread_barrier_wait(&EOS);
    
    // ------- Wait All Boat Threads Join ------- //
    for (int i = 0; i < m; i++) pthread_join(boat_threads[i], NULL);

    // ------- Destroy the barriers ------- //
    pthread_barrier_destroy(&EOS);
    for(int i =0;i<m;i++) pthread_barrier_destroy(&BB[i]);
    
    // ------- Destroy Mutex ------- //
    pthread_mutex_destroy(&bmtx);    
    pthread_mutex_destroy(&(rider.mtx));    
    pthread_mutex_destroy(&(boat.mtx));    
    
    // ------- Free all Variables ------- //
    free(BA);
    free(BC);
    free(BT);
    free(BB);
    free(boat_threads);
    free(rider_threads);
    
    // ------- Exit Main Thread ------- //
    return 0;
}