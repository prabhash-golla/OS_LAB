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

#define MAX_RESOURCE_TYPES 20
#define MAX_THREADS 100
#define rep(i, n) for(int i = 0;i < n; i++)

int m;
int n;
int available[MAX_RESOURCE_TYPES];
int max_need[MAX_THREADS][MAX_RESOURCE_TYPES];
int allocation[MAX_THREADS][MAX_RESOURCE_TYPES];
int need[MAX_THREADS][MAX_RESOURCE_TYPES];
int granted[MAX_THREADS] = {0};
int type;
int who;
int req[MAX_RESOURCE_TYPES];

pthread_barrier_t BOS;
pthread_mutex_t rmtx;
pthread_barrier_t REQB;
pthread_barrier_t *ACKB;
pthread_mutex_t *cmtx;
pthread_cond_t *cv;
pthread_mutex_t pmtx;

typedef struct 
{
    int thread_id;
    int request[MAX_RESOURCE_TYPES];
} Request;

typedef struct QueueNode 
{
    Request req;
    struct QueueNode *next;
} QueueNode;

QueueNode *queue_front = NULL;
QueueNode *queue_rear = NULL;

void enqueue(Request req) 
{
    QueueNode *new_node = (QueueNode *)malloc(sizeof(QueueNode));
    new_node->req = req;
    new_node->next = NULL;
    
    if (queue_rear == NULL) queue_front = queue_rear = new_node;
    else 
    {
        queue_rear->next = new_node;
        queue_rear = new_node;
    }
}


Request dequeue() 
{
    if (queue_front == NULL) 
    {
        Request empty_req;
        memset(&empty_req, 0, sizeof(Request));
        return empty_req;
    }
    
    QueueNode *temp = queue_front;
    Request req = temp->req;
    
    queue_front = queue_front->next;
    
    if (queue_front == NULL) {
        queue_rear = NULL;
    }
    
    free(temp);
    return req;
}

void print_waiting()
{   
    printf("\t\tWaiting threads:");
    fflush(stdout);
    QueueNode *current = queue_front;

    while (current != NULL) 
    {
        printf(" %d",current->req.thread_id);
        fflush(stdout);
        current = current->next;
    }
    printf("\n");
    fflush(stdout);
}



int is_queue_empty() 
{
    return queue_front == NULL;
}

int can_allocate(int thread_id, int request[]) 
{    
    rep(i,m) if (request[i] > need[thread_id][i]) return 0;
    
    rep(i,m) if (request[i] > available[i]) return 0;
    
    
    #ifdef _DLAVOID
    int temp_available[MAX_RESOURCE_TYPES];
    int temp_allocation[MAX_THREADS][MAX_RESOURCE_TYPES];
    int temp_need[MAX_THREADS][MAX_RESOURCE_TYPES];
    
    rep(i,m) temp_available[i] = available[i] - request[i];
    
    rep(i,n)
    {
        rep(j,m) 
        {
            temp_allocation[i][j] = allocation[i][j];
            temp_need[i][j] = need[i][j];
        }
    }
    
    
    rep(i,m) 
    {
        temp_allocation[thread_id][i] += request[i];
        temp_need[thread_id][i] -= request[i];
    }
    
    int finish[MAX_THREADS] = {0};
    int work[MAX_RESOURCE_TYPES];
    
    rep(i,m) work[i] = temp_available[i];
    
    int count = 0;
    while (count < n) 
    {
        int found = 0;
        rep(i,n)
        {
            if (!finish[i]) 
            {
                int can_finish = 1;
                rep(j,m) 
                {
                    if (temp_need[i][j] > work[j]) 
                    {
                        can_finish = 0;
                        break;
                    }
                }
                
                if (can_finish) 
                {
                    rep(j,m) work[j] += temp_allocation[i][j];
                    
                    finish[i] = 1;
                    found = 1;
                    count++;
                }
            }
        }
        
        if (!found)
        {
            return 2;
        }
    }
    #endif

    return 1;
}



void print_vector(int vec[], int size) 
{
    rep(i,size)
    {
        printf(" %d", vec[i]);
        fflush(stdout);
    }
    printf("\n");
    fflush(stdout);
}


void* user_thread(void* id)
{
    int thread_id = *((int *)id);
    free(id);
    
    FILE *thread_file;
    char file_path[100];
    int max_needs[MAX_RESOURCE_TYPES];
    int delay;
    char request_type;
    
    pthread_mutex_lock(&pmtx);
    printf("\tThread %d born\n",thread_id);
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);

    sprintf(file_path, "input/thread%02d.txt", thread_id);
    thread_file = fopen(file_path, "r");
    if (thread_file == NULL) 
    {
        perror("Error opening thread file");
        exit(1);
    }
    
    rep(i,m)
    {
        fscanf(thread_file, "%d", &max_needs[i]);
        max_need[thread_id][i] = max_needs[i];
        need[thread_id][i] = max_needs[i];
    }
    
    pthread_barrier_wait(&BOS);

    while (1) 
    {
        fscanf(thread_file, "%d", &delay);

        if (delay > 0) 
        {
            usleep(delay * 100000);
            fscanf(thread_file, " %c", &request_type);
            if(request_type=='R')
            {
                pthread_mutex_lock(&rmtx);
                
                rep(i,m) fscanf(thread_file, "%d", &req[i]);

                int is_additional = 0;
                rep(i,m)
                {
                    if (req[i] > 0) 
                    {
                        is_additional = 1;
                        break;
                    }
                }
                type = request_type;
                who = thread_id;
                pthread_mutex_lock(&pmtx);
                if (is_additional) printf("\tThread %d sends resource request: type = ADDITIONAL\n", thread_id);
                else printf("\tThread %d sends resource request: type = RELEASE\n", thread_id);
                fflush(stdout);
                pthread_mutex_unlock(&pmtx);

                pthread_barrier_wait(&REQB);

                pthread_barrier_wait(&ACKB[thread_id]);

                pthread_mutex_unlock(&rmtx);
                
                pthread_mutex_lock(&cmtx[thread_id]);
                while (!granted[thread_id]) {
                    pthread_cond_wait(&cv[thread_id], &cmtx[thread_id]);
                }
                granted[thread_id] = 0;
                pthread_mutex_unlock(&cmtx[thread_id]);
                
                pthread_mutex_lock(&pmtx);
                if (is_additional) printf("\tThread %d is granted its last resource request\n", thread_id);
                else printf("\tThread %d is done with its resource release request\n",thread_id);
                fflush(stdout);
                pthread_mutex_unlock(&pmtx);
            }
            else
            {
                pthread_mutex_lock(&rmtx);
                type = request_type;
                who = thread_id;
                pthread_barrier_wait(&REQB);

                pthread_barrier_wait(&ACKB[thread_id]);
                pthread_mutex_unlock(&rmtx);

                pthread_mutex_lock(&pmtx);
                printf("\tThread %d going to quit\n", thread_id);
                fflush(stdout);
                pthread_mutex_unlock(&pmtx);
                pthread_exit(EXIT_SUCCESS);
            }
        }
    }
    
    fclose(thread_file);
    pthread_mutex_lock(&pmtx);
    printf("\tThread %d going to quit\n",thread_id);
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);
}

int main()
{
    pthread_t *user_tids;
    FILE *system_file;
    char file_path[100];

    pthread_mutex_init(&rmtx, NULL);
    pthread_mutex_init(&pmtx, NULL);


    sprintf(file_path, "input/system.txt");
    system_file = fopen(file_path, "r");
    if (system_file == NULL) 
    {
        perror("Error opening system.txt");
        exit(1);
    }

    fscanf(system_file, "%d %d", &m, &n);
    rep(i,m) fscanf(system_file, "%d", available+i);
    fclose(system_file);

    ACKB = (pthread_barrier_t *)malloc(n * sizeof(pthread_barrier_t));
    cmtx = (pthread_mutex_t *)malloc(n * sizeof(pthread_mutex_t));
    cv = (pthread_cond_t *)malloc(n * sizeof(pthread_cond_t));

    rep(i,n)
    {
        pthread_barrier_init(&ACKB[i], NULL, 2);
        pthread_mutex_init(&cmtx[i], NULL);
        pthread_cond_init(&cv[i], NULL);
    }
    
    pthread_barrier_init(&REQB, NULL, 2);
    pthread_barrier_init(&BOS, NULL, n + 1);
    
    rep(i,n)
    {
        rep(j,m)
        {
            allocation[i][j] = 0;
            max_need[i][j] = 0;
            need[i][j] = 0;
        }
    }

    user_tids = (pthread_t *)malloc(n * sizeof(pthread_t));
    
    rep(i,n)
    {
        int *thread_id = (int *)malloc(sizeof(int));
        *thread_id = i;
        pthread_create(&user_tids[i], NULL, user_thread, (void *)thread_id);
    }
    
    
    pthread_barrier_wait(&BOS);

    int remaining_threads = n;
    int new_req[MAX_RESOURCE_TYPES];
    int request_granted;
    int thread[MAX_THREADS];
    memset(thread,0,sizeof(thread));
    char new_type;
    while (1)
    {
        pthread_barrier_wait(&REQB);
        memcpy(new_req, req, sizeof(new_req));
        int request_type = 0; 
        rep(j,m)
        {
            if (new_req[j] > 0) 
            {
                request_type = 1;
                break;
            }
        }
        new_type = type;
        int thread_id = who;
        pthread_barrier_wait(&ACKB[thread_id]);
        
        if(new_type=='R')
        {
            if(!request_type)
            {
                // pthread_mutex_lock(&pmtx);
                // printf("Master thread releases resources request of thread %d\n",thread_id);
                // fflush(stdout);
                // pthread_mutex_unlock(&pmtx);
                rep(j,m)
                {
                    if (new_req[j] < 0) 
                    {
                        available[j] += -new_req[j];
                        allocation[thread_id][j] += new_req[j]; 
                        need[thread_id][j] -= new_req[j]; 
                    }
                }
                pthread_mutex_lock(&cmtx[thread_id]);
                granted[thread_id] = 1;
                pthread_cond_signal(&cv[thread_id]);
                pthread_mutex_unlock(&cmtx[thread_id]);
            }
            else
            {
                rep(j,m) 
                {
                    if (new_req[j] < 0) 
                    {
                        available[j] += -new_req[j];
                        allocation[thread_id][j] += new_req[j]; 
                        need[thread_id][j] -= new_req[j]; 
                        new_req[j] = 0; 
                    }
                }
                Request req;
                req.thread_id = thread_id;
                rep(j,m)
                {
                    req.request[j] = new_req[j];
                }
                enqueue(req);
                pthread_mutex_lock(&pmtx);
                printf("Master thread stores resource request of thread %d\n",thread_id);
                fflush(stdout);
                print_waiting(); 
                pthread_mutex_unlock(&pmtx);
            }
        }
        else
        {
            rep(j,m)
            {
                available[j] += allocation[thread_id][j];
                allocation[thread_id][j] -= allocation[thread_id][j]; 
                need[thread_id][j] += allocation[thread_id][j];
            }
            thread[thread_id]=1;
            remaining_threads--;
            pthread_mutex_lock(&pmtx);
            printf("Master thread releases resources of thread %d\n",thread_id);
            print_waiting(); 
            printf("%d threads left:",remaining_threads);
            rep(i,n)
            {
                if(!thread[i]) printf(" %d",i);
            }
            printf("\nAvailable resources:");
            print_vector(available,m);
            fflush(stdout);
            pthread_mutex_unlock(&pmtx);
            if(!remaining_threads) break;
        }

        pthread_mutex_lock(&pmtx);
        printf("Master thread tries to grant pending requests\n");
        fflush(stdout);
        pthread_mutex_unlock(&pmtx);

        int f_ftid =-1;
        while (!is_queue_empty()) 
        {
            QueueNode *front = queue_front;
            if(f_ftid==-1) f_ftid = front->req.thread_id;
            else if(f_ftid==front->req.thread_id) break;
            int peek_thread_id = front->req.thread_id;
            request_granted = can_allocate(peek_thread_id, front->req.request);
            
            Request granted_req = dequeue();
            if (request_granted==1) 
            {
                pthread_mutex_lock(&pmtx);
                printf("Master thread grants resource request for thread %d\n", granted_req.thread_id);
                fflush(stdout);
                pthread_mutex_unlock(&pmtx);
                rep(j,m)
                {
                    available[j] -= granted_req.request[j];
                    allocation[granted_req.thread_id][j] += granted_req.request[j];
                    need[granted_req.thread_id][j] -= granted_req.request[j];
                }
        
                pthread_mutex_lock(&cmtx[granted_req.thread_id]);
                granted[granted_req.thread_id] = 1;
                pthread_cond_signal(&cv[granted_req.thread_id]);
                pthread_mutex_unlock(&cmtx[granted_req.thread_id]);
                if(f_ftid==granted_req.thread_id) f_ftid=-1;
            } 
            else 
            {
                pthread_mutex_lock(&pmtx);
                if(request_granted==0) printf("    +++ Insufficient resources to grant request of thread %d\n", peek_thread_id);
                else if(request_granted==2) printf("    +++ Unsafe to grant request of thread %d\n", peek_thread_id);
                fflush(stdout);
                pthread_mutex_unlock(&pmtx);
                enqueue(granted_req);
            }
        }

        pthread_mutex_lock(&pmtx);
        print_waiting();
        pthread_mutex_unlock(&pmtx);

    }
    

    rep(i,n) pthread_join(user_tids[i], NULL);

    rep(i,n)
    {
        pthread_barrier_destroy(&ACKB[i]);
        pthread_mutex_destroy(&cmtx[i]);
        pthread_cond_destroy(&cv[i]);
    }
    
    free(user_tids);
    free(ACKB);
    free(cmtx);
    free(cv);

    pthread_barrier_destroy(&REQB);
    pthread_barrier_destroy(&BOS);
    pthread_mutex_destroy(&rmtx);
    pthread_mutex_destroy(&pmtx);
    
    return 0;
}