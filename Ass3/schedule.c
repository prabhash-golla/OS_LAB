#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

// Process structure
typedef struct 
{
    int id;
    int time;
    int type;       // 0: Arrival, 1: CPU-end, 4: Timeout, 2: IO-completion,3: First Arrival
    int arrival_time;
    int *bursts; // Array of CPU/IO burst times
    int num_bursts;
    int current_burst;
    int state; // 0: New, 2: Ready, 6: Running, 3: IO, 4: Terminated , 5: Join after Time Out
    int burst_time;
} Process;

// Queue structure
typedef struct 
{
    int *data;
    int front;
    int rear;
    int capacity;
} Queue;


// Queue functions
void init_queue(Queue *q, int capacity) 
{
    q->data = malloc(capacity * sizeof(int));
    q->front = 0;
    q->rear = 0;
    q->capacity = capacity;
}

void enqueue(Queue *q, int value)
{
    // printf("Enqueue : %d\n",value+1);
    q->data[q->rear++] = value;
    if (q->rear == q->capacity) q->rear = 0;
}

int dequeue(Queue *q) 
{
    int value = q->data[q->front++];
    // printf("Dequeue : %d\n",value+1);
    if (q->front == q->capacity) q->front = 0;
    return value;
}

int is_empty(Queue *q) 
{
    return q->front == q->rear;
}

// Min-heap functions
void heapify(int heap[], int n,Process *processes)
{
    int smallest = 0;
    for (int i = n / 2 - 1; i >= 0; i--) 
    {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        smallest = i;
        // Compare left child
        if (left < n && (processes[heap[left]].time < processes[heap[smallest]].time || (processes[heap[left]].time == processes[heap[smallest]].time && processes[heap[left]].type < processes[heap[smallest]].type) || (processes[heap[left]].time == processes[heap[smallest]].time && processes[heap[left]].type == processes[heap[smallest]].type && heap[left] < heap[smallest]))) smallest = left;
        // Compare right child
        if (right < n && (processes[heap[right]].time < processes[heap[smallest]].time || (processes[heap[right]].time == processes[heap[smallest]].time && processes[heap[right]].type < processes[heap[smallest]].type) || (processes[heap[right]].time == processes[heap[smallest]].time && processes[heap[right]].type == processes[heap[smallest]].type && heap[right] < heap[smallest]))) smallest = right;

        if (smallest != i) 
        {
            int temp = heap[i];
            heap[i] = heap[smallest];
            heap[smallest] = temp;
        }
    }
}

void insert_event(int heap[], int *size, int new_event,Process *processes) 
{
    heap[*size] = new_event;
    (*size)++;
    for (int i = (*size) / 2 - 1; i >= 0; i--) heapify(heap, *size,processes);
}

int extract_min(int heap[], int *size,Process *processes) 
{
    int min_event = heap[0];
    heap[0] = heap[--(*size)];
    heapify(heap, *size,processes);
    return min_event;
}

// Read input file
Process* read_input_file(const char *filename, int *num_processes) 
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "%d", num_processes);
    Process *processes = malloc((*num_processes) * sizeof(Process));
    for (int i = 0; i < *num_processes; i++) 
    {
        fscanf(file, "%d %d", &processes[i].id, &processes[i].arrival_time);
        int j=0,k=0;
        processes[i].bursts = malloc(20 * sizeof(int));
        processes[i].burst_time=1;
        while(k!=-1)
        {
            
            fscanf(file, "%d", &k);
            processes[i].bursts[j++]=k;
            processes[i].burst_time+=k;
        }
        processes[i].num_bursts = j;
        processes[i].state = 0;
        processes[i].current_burst = 0;
    }

    fclose(file);
    return processes;
}

// Simulation
void simulate_rr(Process *processes, int num_processes, int quantum) 
{
    int event_count = 0;
    int event_queue[1000];
    Queue ready_queue;
    init_queue(&ready_queue, num_processes+1);

    if(quantum == INT_MAX-1) printf("**** FCFS Scheduling ****\n");
    else printf("**** RR Scheduling with q = %d ****\n",quantum);

    #ifdef VERBOSE
    printf("%-10d: Starting\n",0);
    #endif

    // Initialize event queue with arrivals
    for (int i = 0; i < num_processes; i++)
    {
        processes[i].time = processes[i].arrival_time;
        processes[i].type = 3;
        insert_event(event_queue, &event_count,i,processes);
    }

    int k=0;
    int total_idle_time = 0;
    int total_wait_time = 0;
    int total_turnaround_time = 0;
    int idle_new;
    int p=0;

    while (event_count > 0 || !is_empty(&ready_queue))
    {
        int current_event = extract_min(event_queue, &event_count,processes);
        int current_time = processes[current_event].time;
        switch (processes[current_event].type)
        {
            case 5:
                #ifdef VERBOSE
                    printf("%-10d: Process %d joins ready queue after timeout\n",processes[current_event].time,current_event+1);
                #endif
                k=0;
            case 3:
                if(processes[current_event].type==3)
                {
                    #ifdef VERBOSE
                        printf("%-10d: Process %d joins ready queue upon arrival\n",processes[current_event].time,current_event+1);
                    #endif
                }
            case 0:
                enqueue(&ready_queue,current_event);    
                break;
        
            case 6:
                k=0;
                if(processes[current_event].bursts[processes[current_event].current_burst]==-1)
                {
                    #ifdef VERBOSE
                    printf("%-10d: Process      %d exits.\n",processes[current_event].time,current_event+1);         
                    #endif
                    break;
                }
                processes[current_event].type = 1;
                processes[current_event].time = processes[current_event].time+processes[current_event].bursts[processes[current_event].current_burst++];
                insert_event(event_queue,&event_count,current_event,processes);
                break; 

            case 1:
                #ifdef VERBOSE
                printf("%-10d: Process %d joins ready queue after IO completion\n",processes[current_event].time,current_event+1);
                #endif
                enqueue(&ready_queue, current_event);
                break;

            case 4:
                int t1 = processes[current_event].time-processes[current_event].arrival_time;
                int t2 =  processes[current_event].time-processes[current_event].arrival_time-processes[current_event].burst_time;
                total_wait_time+=t2;
                total_turnaround_time+=t1;
                printf("%-10d: Process %6d exits. Turnaround time = %4d (%d%%), Wait time = %d\n",processes[current_event].time,current_event+1,t1,(int)(100.0*(t1)/(t1-t2)+0.5),t2);
                total_turnaround_time =processes[current_event].time;
                k=0;
                break;
            default:
                break;
        }
        if (!is_empty(&ready_queue) && !k) 
        {
            if(p) total_idle_time+=current_time-idle_new;
            p=0;
            k=1;
            int next_process = dequeue(&ready_queue);
            processes[next_process].state = 6; // Running
            int burst_time = processes[next_process].bursts[processes[next_process].current_burst];
            if (burst_time > quantum) 
            {
                #ifdef VERBOSE
                    printf("%-10d: Process %d is scheduled to run for time %d\n",current_time,next_process+1,quantum);
                #endif
                processes[next_process].time = current_time + quantum;
                processes[next_process].type = 5;
                insert_event(event_queue, &event_count, next_process,processes);
                processes[next_process].bursts[processes[next_process].current_burst]-=quantum;
            } 
            else 
            {
                #ifdef VERBOSE
                    printf("%-10d: Process %d is scheduled to run for time %d\n",current_time,next_process+1,processes[next_process].bursts[processes[next_process].current_burst]);
                #endif
                if(processes[next_process].bursts[processes[next_process].current_burst+1]==-1)
                {
                    processes[next_process].time = current_time + burst_time;
                    processes[next_process].type = 4;
                    insert_event(event_queue, &event_count, next_process,processes);
                }
                else
                {
                    processes[next_process].time = current_time + burst_time;
                    processes[next_process].type = 6;
                    insert_event(event_queue, &event_count, next_process,processes);
                }
                // printf("Prabhash: %d %d \n",processes[next_process].time,next_process);
                processes[next_process].bursts[processes[next_process].current_burst]=0;
                processes[next_process].current_burst++;
            }
        }
        else if(!k)
        {
            idle_new = current_time;
            p=1;
            #ifdef VERBOSE
            printf("%-10d: CPU goes idle\n",current_time);
            #endif
        }
    }
    printf("\nAverage wait time = %.2lf\n",1.0*total_wait_time/num_processes);
    printf("Total turnaround time = %d\n",total_turnaround_time);
    printf("CPU idle time = %d\n",total_idle_time);
    printf("CPU utilization = %.2lf%%\n\n",100.0*(total_turnaround_time-total_idle_time)/total_turnaround_time);
}

int main() 
{
    int num_processes;
    Process *processes = read_input_file("proc.txt", &num_processes);
    simulate_rr(processes, num_processes, INT_MAX-1);
    processes = read_input_file("proc.txt", &num_processes);
    simulate_rr(processes, num_processes, 10);
    processes = read_input_file("proc.txt", &num_processes);
    simulate_rr(processes, num_processes, 5);
    for (int i = 0; i < num_processes; i++) free(processes[i].bursts);
    free(processes);

    return 0;
}
