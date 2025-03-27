#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#define PAGE_SIZE 4096
#define PAGE_TABLE_ENTRIES 2048
#define OS_RESERVED_FRAMES 4096
#define TOTAL_FRAMES 16384
#define USER_FRAMES TOTAL_FRAMES-OS_RESERVED_FRAMES

/*--- Int Queue --- */
/* --- Node Structure --- */
typedef struct intNode 
{
    int data;
    struct intNode* next;
} intNode;

/* --- Integer Queue implemented as a Linked List --- */
typedef struct 
{
    intNode* front;
    intNode* rear;
    int count;
} IntQueue;

/* Initialize the linked list queue */
void initIntQueue(IntQueue* q) 
{
    q->front = NULL;
    q->rear = NULL;
    q->count = 0;
}

/* Check if the queue is empty */
int isEmptyIntQueue(IntQueue* q) 
{
    return (q->front == NULL);
}

/* Push an integer value into the queue */
void pushIntQueue(IntQueue* q, int val) 
{
    intNode* newNode = (intNode*)malloc(sizeof(intNode));
    if (!newNode) 
    {
        fprintf(stderr, "Memory allocation error for Node\n");
        exit(1);
    }
    newNode->data = val;
    newNode->next = NULL;
    
    if (q->rear == NULL) 
    {
        // Queue is empty
        q->front = newNode;
        q->rear = newNode;
    }
    else 
    {
        q->rear->next = newNode;
        q->rear = newNode;
    }
    q->count++;
}

/* Pop an integer value from the queue */
int popIntQueue(IntQueue* q) 
{
    if (isEmptyIntQueue(q)) 
    {
        fprintf(stderr, "Free frames queue underflow\n");
        exit(1);
    }
    
    intNode* temp = q->front;
    int val = temp->data;
    q->front = temp->next;
    
    if (q->front == NULL) q->rear = NULL;
    free(temp);
    q->count--;
    return val;
}

/* Free all nodes in the queue */
void freeIntQueue(IntQueue* q) 
{
    while (!isEmptyIntQueue(q)) popIntQueue(q);
}


/* --- Process --- */

/* --- Process structure --- */ 
typedef struct 
{
    int Id;
    int s;
    int m;
    int *keys;              
    int current_search;
    uint16_t *page_table;
} Process;

/* --- Process Queue Node --- */
typedef struct ProcessNode 
{
    Process *process;
    struct ProcessNode *next;
} ProcessNode;

/* --- Process Queue Stucture --- */
typedef struct 
{
    ProcessNode* head;
    ProcessNode* tail;
    int count;
} ProcessQueue;

/* --- Init Process Queue --- */
void initProcessQueue(ProcessQueue *q) 
{
    q->head = NULL;
    q->tail = NULL;
    q->count = 0;
}

/* --- isEmpty Process Queue --- */
int isEmptyProcessQueue(ProcessQueue *q) 
{
    return (q->head == NULL);
}

/* --- Push operation on Process Queue --- */
void pushProcessQueue(ProcessQueue *q, Process *p) 
{
    ProcessNode *node = (ProcessNode *)malloc(sizeof(ProcessNode));
    if (!node) 
    {
        fprintf(stderr, "Memory allocation error for ProcessNode\n");
        exit(1);
    }
    node->process = p;
    node->next = NULL;
    if (q->tail == NULL) q->head = q->tail = node; 
    else 
    {
        q->tail->next = node;
        q->tail = node;
    }
    q->count++;
}

/* --- Push In Front of Process Queue --- */
void pushFrontProcessQueue(ProcessQueue *q, Process *p) 
{
    ProcessNode *node = (ProcessNode *)malloc(sizeof(ProcessNode));
    if (!node) 
    {
        fprintf(stderr, "Memory allocation error for ProcessNode\n");
        exit(1);
    }
    node->process = p;
    node->next = q->head;  // New node points to current head
    q->head = node;        // New node becomes the new head
    
    // If the queue was empty, set the tail as well
    if (q->tail == NULL) 
    {
        q->tail = node;
    }
    q->count++;
}

/* --- Pop from Process Queue --- */
Process* popProcessQueue(ProcessQueue *q) 
{
    if (q->head == NULL) 
    {
        fprintf(stderr, "Process queue underflow\n");
        exit(1);
    }
    ProcessNode *node = q->head;
    Process *p = node->process;
    q->head = node->next;
    if (q->head == NULL) q->tail = NULL;
    free(node);
    q->count--;
    return p;
}

int main()
{
    /* --- Queues --- */
    IntQueue free_frames;
    ProcessQueue ready_queue;
    ProcessQueue swapped_processes;
    
    /* --- variables --- */
    int page_accesses = 0;
    int page_faults = 0;
    int swap_count = 0;
    int degree_min = INT_MAX;
    int n_processes = 0;
    int completed_count = 0;
    
    initIntQueue(&free_frames);
    initProcessQueue(&ready_queue);
    initProcessQueue(&swapped_processes);

    for (int i = 0; i < USER_FRAMES; i++) pushIntQueue(&free_frames, OS_RESERVED_FRAMES + i);

    FILE *file = fopen("search.txt", "r");
    if (!file) 
    {
        fprintf(stderr, "Error opening file %s\n", "search.txt");
        exit(1);
    }

    int n, m;
    if (fscanf(file, "%d %d", &n, &m) != 2) 
    {
        fprintf(stderr, "Error reading process counts\n");
        exit(1);
    }

    n_processes = n;
    Process *processes = (Process *)malloc(n * sizeof(Process));
    if (!processes) 
    {
        fprintf(stderr, "Memory allocation error for processes\n");
        exit(1);
    }

    for (int i = 0; i < n; i++) 
    {
        processes[i].Id = i;
        if (fscanf(file, "%d", &processes[i].s) != 1) 
        {
            fprintf(stderr, "Error reading process s\n");
            exit(1);
        }

        processes[i].m = m;
        processes[i].keys = (int *)malloc(m * sizeof(int));
        if (!processes[i].keys) 
        {
            fprintf(stderr, "Memory allocation error for keys\n");
            exit(1);
        }

        for (int j = 0; j < m; j++) 
        {
            if (fscanf(file, "%d", &processes[i].keys[j]) != 1)
            {
                fprintf(stderr, "Error reading key\n");
                exit(1);
            }
        }

        processes[i].current_search = 0;
        processes[i].page_table = (uint16_t *)calloc(PAGE_TABLE_ENTRIES, sizeof(uint16_t));
        if (!processes[i].page_table) 
        {
            fprintf(stderr, "Memory allocation error for page_table\n");
            exit(1);
        }

        // Allocate 10 essential frames
        for (int j = 0; j < 10; j++) 
        {
            if (free_frames.count == 0) 
            {
                fprintf(stderr, "Not enough free frames during init\n");
                exit(1);
            }
            int frame = popIntQueue(&free_frames);
            processes[i].page_table[j] = 0x8000 | frame;
        }
        pushProcessQueue(&ready_queue, &processes[i]);
    }
    fclose(file);

    printf("+++ Simulation data read from file\n");
    printf("+++ Kernel data initialized\n");

    /* --- Simulation --- */
    while (1) 
    {
        if (isEmptyProcessQueue(&ready_queue)) 
        {
            if (completed_count == n_processes) break;
            fprintf(stderr, "Deadlock: no ready processes but not all completed.\n");
            exit(1);
        }

        Process *p = popProcessQueue(&ready_queue);

        if (p->current_search >= p->m) continue;

        #ifdef VERBOSE
        printf("\tSearch %d by Process %d\n", p->current_search + 1, p->Id);
        #endif

        int key = p->keys[p->current_search];
        int L = 0, R = p->s - 1;
        int swapped_out = 0;

        while (L < R && !swapped_out) 
        {
            int M = (L + R) / 2;
            int page_num = 10 + (M / 1024);

            // Count every access to A[M]
            page_accesses++;

            if ((p->page_table[page_num] & 0x8000) == 0) 
            {
                page_faults++;
                if (!isEmptyIntQueue(&free_frames)) 
                {
                    int frame = popIntQueue(&free_frames);
                    p->page_table[page_num] = 0x8000 | frame;
                } 
                else 
                {
                    swap_count++;
                    // Free all frames allocated to this process
                    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) 
                    {
                        if (p->page_table[i] & 0x8000) 
                        {
                            int frame = p->page_table[i] & 0x3FFF;
                            pushIntQueue(&free_frames, frame);
                            p->page_table[i] = 0;
                        }
                    }
                    pushProcessQueue(&swapped_processes, p);

                    int swapped_size = swapped_processes.count;
                    int active = n_processes - swapped_size - completed_count;
                    if (active < degree_min) degree_min = active;
                    printf("+++ Swapping out process %4d  [%d active processes]\n", p->Id, active);
                    swapped_out = 1;
                    break;
                }
            }
            if (key <= M) R = M;
            else L = M + 1;
        }

        if (!swapped_out) 
        {
            p->current_search++;

            if (p->current_search == p->m) 
            {
                completed_count++;
                // Free all frames when process completes
                for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) 
                {
                    if (p->page_table[i] & 0x8000) 
                    {
                        int frame = p->page_table[i] & 0x3FFF;
                        pushIntQueue(&free_frames, frame);
                        p->page_table[i] = 0;
                    }
                }

                // Swap in a process if any are waiting
                if (!isEmptyProcessQueue(&swapped_processes)) 
                {
                    Process *q = popProcessQueue(&swapped_processes);
                    for (int i = 0; i < 10; i++) {
                        if (free_frames.count == 0) {
                            fprintf(stderr, "No free frames when swapping in.\n");
                            exit(1);
                        }
                        int frame = popIntQueue(&free_frames);
                        q->page_table[i] = 0x8000 | frame;
                    }
                    int swapped_size = swapped_processes.count;
                    int active = n_processes - swapped_size - completed_count;
                    printf("+++ Swapping in process  %4d  [%d active processes]\n", q->Id, active);
                    pushFrontProcessQueue(&ready_queue, q);
                }
            } 
            else pushProcessQueue(&ready_queue, p);
        }
    }

    printf("+++ Page access summary\n");
    printf("\tTotal number of page accesses  =  %d\n", page_accesses);
    printf("\tTotal number of page faults    =  %d\n", page_faults);
    printf("\tTotal number of swaps          =  %d\n", swap_count);
    printf("\tDegree of multiprogramming     =  %d\n", degree_min);

    // Free allocated memory for each process
    for (int i = 0; i < n_processes; i++) 
    {
        free(processes[i].keys);
        free(processes[i].page_table);
    }
    free(processes);

    // Free any remaining nodes in the process queues (if any)
    ProcessNode *node;
    while ((node = ready_queue.head) != NULL) 
    {
        ready_queue.head = node->next;
        free(node);
    }
    while ((node = swapped_processes.head) != NULL) 
    {
        swapped_processes.head = node->next;
        free(node);
    }

    return 0;
}