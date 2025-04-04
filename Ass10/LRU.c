#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

/*--- Constants ---*/
#define PAGE_SIZE 4096                        // Size of a page/frame in bytes
#define PAGE_TABLE_ENTRIES 2048               // Number of page table entries per process
#define OS_RESERVED_FRAMES 4096               // Frames reserved for OS usage
#define TOTAL_FRAMES 16384                    // Total frames in the system
#define USER_FRAMES (TOTAL_FRAMES - OS_RESERVED_FRAMES)  // Frames available for user processes
#define NFFMIN 1000                           // Minimum number of free frames allowed

/*--- Bit masks ---*/
#define VALID_BIT    0x8000                   // Bit 15: valid bit (1 means valid)
#define REFERENCE_BIT 0x4000                  // Bit 14: reference bit
#define FRAME_MASK    0x3FFF                  // Bits 0-13: frame address

#define INITIAL_HISTORY 0xffff                // Initial history value when a page is loaded

/*--- Free Frame List Structure ---*/
typedef struct 
{
    int frame;       // Frame number
    int last_owner;  // Process ID of the last owner (-1 means no owner)
    int last_page;   // Last page number stored in this frame (-1 means none)
} FreeFrameEntry;

/*--- Process Structure ---*/
typedef struct 
{
    int Id;                // Process ID
    int s;                 // Size (used in binary search range)
    int m;                 // Number of binary searches
    int *keys;             // Array of keys for searches
    int current_search;    // Current search number
    int page_access;       // Total page accesses for this process
    int page_faults;       // Total page faults for this process
    int page_replacements; // Total page replacements for this process
    int attempt1;          // Count of successful replacement attempt 1
    int attempt2;          // Count of successful replacement attempt 2
    int attempt3;          // Count of successful replacement attempt 3
    int attempt4;          // Count of successful replacement attempt 4
    uint16_t *page_table;  // Page table entries (16-bit each)
    uint16_t *history;     // History for each page (for approximate LRU)
} Process;

FreeFrameEntry *free_frame_list;  // Global free frame list array
int free_frame_count;             // Current number of free frames in the list
Process *processes;               // Array of processes

/*--- Functions for Free Frame List ---*/
void init_free_frame_list() 
{
    free_frame_count = USER_FRAMES;  // Initially, all user frames are free
    free_frame_list = (FreeFrameEntry *) malloc(USER_FRAMES * sizeof(FreeFrameEntry));
    if (!free_frame_list) 
    {
        fprintf(stderr, "Memory allocation error for free_frame_list\n");
        exit(1);
    }
    for (int i = 0; i < USER_FRAMES; i++) 
    {
        // Initialize each free frame entry with frame number and no owner/page
        free_frame_list[i].frame =  i;
        free_frame_list[i].last_owner = -1;
        free_frame_list[i].last_page = -1;
    }
}

int allocate_free_frame_normal(int page_num,int pid) 
{
    if (free_frame_count <= NFFMIN) 
    {
        fprintf(stderr, "Error: Not enough free frames for normal allocation\n");
        exit(1);
    }
    // Remove and return the last frame in the free_frame_list.
    int frame = free_frame_list[USER_FRAMES-free_frame_count].frame;
    free_frame_list[USER_FRAMES-free_frame_count].last_owner = pid;  // Update owner info
    #ifdef VERBOSE
        if(page_num != -1) printf("    Fault on Page %4d: Free frame %d found\n", page_num, frame); 
    #endif
    free_frame_count--;  // Decrement free frame count
    return frame;
}

void free_frame(int frame, int owner, int page) 
{
    // Add the frame back to free_frame_list with given owner and page info
    free_frame_list[free_frame_count].frame = frame;
    free_frame_list[free_frame_count].last_owner = owner;
    free_frame_list[free_frame_count].last_page = page;
    free_frame_count++;  // Increment free frame count
}

/* Replacement attempt counters (for statistics) */
int attempt1 = 0, attempt2 = 0, attempt3 = 0, attempt4 = 0;

int allocate_free_frame_replacement(int procId, int faulting_page, int hist) 
{
    // Attempt 1: Look for a free frame with last_owner == procId and last_page == faulting_page.
    for (int i = TOTAL_FRAMES-OS_RESERVED_FRAMES-1; i >= 0; i--) 
    {
        if (free_frame_list[i].last_owner == procId && free_frame_list[i].last_page == faulting_page) 
        {
            int frame = free_frame_list[i].frame;
            #ifdef VERBOSE
                // Print candidate free frame details using the candidate index (i)
                printf("    Fault on Page %4d: To replace Page %d at Frame %d [history = %d]\n",
                       faulting_page,
                       free_frame_list[free_frame_count-1].last_page,
                       free_frame_list[free_frame_count-1].frame,
                       hist); 
                printf("        Attempt 1: Page found in free frame %d\n", frame);
            #endif
            free_frame_list[i] = free_frame_list[free_frame_count-1];  // Replace candidate with last element
            free_frame_count--;  // Decrement free frame count
            attempt1++;
            processes[procId].attempt1++;
            return frame;
        }
    }

    // Attempt 2: Look for a free frame with no owner.
    for (int i = TOTAL_FRAMES-OS_RESERVED_FRAMES-1; i >= 0; i--) 
    {
        if (free_frame_list[i].last_owner == -1) 
        {
            int frame = free_frame_list[i].frame;
            #ifdef VERBOSE
                printf("    Fault on Page %4d: To replace Page %d at Frame %d [history = %d]\n",
                       faulting_page,
                       free_frame_list[free_frame_count-1].last_page,
                       free_frame_list[free_frame_count-1].frame,
                       hist); 
                printf("        Attempt 2: Free frame %d owned by no process found\n", frame);
            #endif   
            free_frame_list[i] = free_frame_list[free_frame_count-1];
            free_frame_count--;
            attempt2++;
            processes[procId].attempt2++;
            return frame;
        }
    }

    // Attempt 3: Look for a free frame with last_owner == procId.
    for (int i = TOTAL_FRAMES-OS_RESERVED_FRAMES; i >= 0; i--) 
    {
        if (free_frame_list[i].last_owner == procId ) 
        {
            if(free_frame_list[i].last_page == -1) 
            {
                // If the candidate has no specific last_page, use Attempt 4 instead.
                int frame = free_frame_list[i].frame;
                #ifdef VERBOSE
                    printf("    Fault on Page %4d: To replace Page %d at Frame %d [history = %d]\n",
                           faulting_page,
                           free_frame_list[free_frame_count-1].last_page,
                           free_frame_list[free_frame_count-1].frame,
                           hist); 
                    printf("        Attempt 4: Free frame %d owned by Process %d chosen\n",
                           frame, free_frame_list[i].last_owner);
                #endif 
                free_frame_list[i] = free_frame_list[free_frame_count - 1];
                free_frame_count--;
                attempt4++;
                processes[procId].attempt4++;
                return frame;
            } 
            int frame = free_frame_list[i].frame;
            #ifdef VERBOSE
                printf("    Fault on Page %4d: To replace Page %d at Frame %d [history = %d]\n",
                       faulting_page,
                       free_frame_list[free_frame_count-1].last_page,
                       free_frame_list[free_frame_count-1].frame,
                       hist); 
                printf("        Attempt 3: Own page %d found in free frame %d\n",
                       free_frame_list[i].last_page, frame);
            #endif 
            free_frame_list[i] = free_frame_list[free_frame_count - 1];
            free_frame_count--;
            attempt3++;
            processes[procId].attempt3++;
            return frame;
        }
    }
    return 0;  // Return 0 if no frame is found (should not happen if there is always a candidate)
}

/*--- Process Queue (for round-robin scheduling) ---*/
typedef struct ProcessNode 
{
    Process *process;  // Pointer to a process in the queue
    struct ProcessNode *next;
} ProcessNode;

typedef struct 
{
    ProcessNode *head;  // Front of the queue
    ProcessNode *tail;  // End of the queue
    int count;          // Number of processes in the queue
} ProcessQueue;

void initProcessQueue(ProcessQueue *q) 
{
    q->head = q->tail = NULL;
    q->count = 0;
}

int isEmptyProcessQueue(ProcessQueue *q) 
{
    return (q->head == NULL);  // Returns true if queue is empty
}

void pushProcessQueue(ProcessQueue *q, Process *p) 
{
    ProcessNode *node = (ProcessNode *) malloc(sizeof(ProcessNode));
    if (!node) {
        fprintf(stderr, "Memory allocation error for ProcessNode\n");
        exit(1);
    }
    node->process = p;
    node->next = NULL;
    if (q->tail == NULL)
        q->head = q->tail = node;
    else 
    {
        q->tail->next = node;
        q->tail = node;
    }
    q->count++;  // Increase count of processes in queue
}

Process* popProcessQueue(ProcessQueue *q) 
{
    if (q->head == NULL) {
        fprintf(stderr, "Process queue underflow\n");
        exit(1);
    }
    ProcessNode *node = q->head;
    Process *p = node->process;
    q->head = node->next;
    if (q->head == NULL)
        q->tail = NULL;
    free(node);
    q->count--;  // Decrease count of processes in queue
    return p;
}

/*--- Main Simulation ---*/
int main() 
{
    int page_accesses = 0;         // Global page access counter
    int page_faults = 0;           // Global page fault counter
    int page_replacements = 0;     // Global page replacement counter
    int degree_min = INT_MAX;      // Minimum degree of multiprogramming observed
    int n_processes = 0;           // Number of processes
    int completed_count = 0;       // Number of processes that have completed

    ProcessQueue ready_queue;
    initProcessQueue(&ready_queue);  // Initialize the ready queue

    /* Initialize global free frame list */
    init_free_frame_list();

    FILE *file = fopen("search.txt", "r");
    if (!file) {
        fprintf(stderr, "Error opening file search.txt\n");
        exit(1);
    }
    int n, m;
    if (fscanf(file, "%d %d", &n, &m) != 2) {
        fprintf(stderr, "Error reading process counts\n");
        exit(1);
    }
    n_processes = n;

    processes = (Process *) malloc(n * sizeof(Process));
    if (!processes) {
        fprintf(stderr, "Memory allocation error for processes\n");
        exit(1);
    }

    /* Read process data and initialize each process */
    for (int i = 0; i < n; i++) 
    {
        processes[i].Id = i;
        if (fscanf(file, "%d", &processes[i].s) != 1) 
        {
            fprintf(stderr, "Error reading process s\n");
            exit(1);
        }
        processes[i].m = m;
        processes[i].keys = (int *) malloc(m * sizeof(int));
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
        processes[i].page_access = 0;
        processes[i].page_faults = 0;
        processes[i].page_replacements = 0;
        processes[i].attempt1 = 0;
        processes[i].attempt2 = 0;
        processes[i].attempt3 = 0;
        processes[i].attempt4 = 0;
        processes[i].page_table = (uint16_t *) calloc(PAGE_TABLE_ENTRIES, sizeof(uint16_t));
        if (!processes[i].page_table) 
        {
            fprintf(stderr, "Memory allocation error for page_table\n");
            exit(1);
        }
        processes[i].history = (uint16_t *) calloc(PAGE_TABLE_ENTRIES, sizeof(uint16_t));
        if (!processes[i].history) 
        {
            fprintf(stderr, "Memory allocation error for history\n");
            exit(1);
        }
        /* Allocate 10 essential frames (pages 0 to 9) for the process */
        for (int j = 0; j < 10; j++) 
        {
            int frame = allocate_free_frame_normal(-1, i);
            processes[i].page_table[j] = VALID_BIT | frame;
            processes[i].history[j] = INITIAL_HISTORY;
        }
        pushProcessQueue(&ready_queue, &processes[i]);  // Enqueue the process for execution
    }
    fclose(file);
    
    /* Simulation loop: each process performs binary searches */
    while (completed_count < n_processes) 
    {
        if (isEmptyProcessQueue(&ready_queue)) 
        {
            fprintf(stderr, "Deadlock: no ready processes but not all completed.\n");
            exit(1);
        }
        Process *p = popProcessQueue(&ready_queue);
        if (p->current_search >= p->m) continue; // Skip if process has completed all searches

        #ifdef VERBOSE
            printf("+++ Process %d: Search %d \n", p->Id, p->current_search + 1);
        #endif

        int key = p->keys[p->current_search];
        int L = 0, R = p->s - 1;

        /* Binary search loop */
        while (L < R) 
        {
            int M = (L + R) / 2;
            /* Compute the page number for A[M]: pages 10 .. 2047 store the array */
            int page_num = 10 + (M / 1024);
            page_accesses++;
            p->page_access++;

            if ((p->page_table[page_num] & VALID_BIT) == 0) 
            {
                /* Page fault encountered for this page */
                page_faults++;
                p->page_faults++;
                if (free_frame_count > NFFMIN) 
                {
                    int frame = allocate_free_frame_normal(page_num, p->Id);
                    p->page_table[page_num] = VALID_BIT | frame;
                    p->history[page_num] = INITIAL_HISTORY;
                }
                else 
                {
                    /* Page replacement is required when free_frame_count <= NFFMIN */
                    page_replacements++;
                    p->page_replacements++;
                    /* Select victim page among non-essential pages (pages 10..) with minimum history */
                    int victim = -1;
                    uint16_t min_history = 0xffff;

                    for (int i = 10; i < PAGE_TABLE_ENTRIES; i++) 
                    {
                        if (p->page_table[i] & VALID_BIT) 
                        {
                            if (p->history[i] < min_history) 
                            {
                                min_history = p->history[i];
                                victim = i;
                            }
                        }
                    }

                    if (victim == -1) 
                    {
                        fprintf(stderr, "No victim found for replacement in process %d\n", p->Id);
                        exit(1);
                    }

                    /* Free the victim page and record its history */
                    int victim_frame = p->page_table[victim] & FRAME_MASK;
                    p->page_table[victim] = 0;
                    int hist = p->history[victim];  // Capture victim's history for verbose printing
                    p->history[victim] = 0;
                    free_frame(victim_frame, p->Id, victim);
                    
                    /* Allocate a new frame via replacement attempts */
                    int new_frame = allocate_free_frame_replacement(p->Id, page_num, hist);
                    p->page_table[page_num] = VALID_BIT | new_frame;
                    p->history[page_num] = INITIAL_HISTORY;
                }
            }
            /* Set the reference bit on the page upon access */
            p->page_table[page_num] |= REFERENCE_BIT;

            if (key <= M)
                R = M;
            else 
                L = M + 1;
        } /* end binary search loop */

        /* Update history for all valid pages of process p after completing the search */
        for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) 
        {
            if (p->page_table[i] & VALID_BIT) 
            {
                if (i < 10)
                    p->history[i] = INITIAL_HISTORY; // Essential pages always retain INITIAL_HISTORY
                else 
                {
                    uint16_t ref = (p->page_table[i] & REFERENCE_BIT) ? 1 : 0;
                    // Right shift history and insert current reference bit in the MSB (bit 15)
                    p->history[i] = (p->history[i] >> 1) | (ref << 15);
                }
                /* Clear the reference bit after updating history */
                p->page_table[i] &= ~REFERENCE_BIT;
            }
        }

        p->current_search++;
        if (p->current_search == p->m) 
        {
            completed_count++;
            /* Process has finished; free all its allocated frames */
            for (int i = 0; i < PAGE_TABLE_ENTRIES; i++) 
            {
                if (p->page_table[i] & VALID_BIT) 
                {
                    int frame = p->page_table[i] & FRAME_MASK;
                    free_frame(frame, -1, -1);
                    p->page_table[i] = 0;
                    p->history[i] = 0;
                }
            }
        } 
        else 
            pushProcessQueue(&ready_queue, p);
        
        int active = n_processes - completed_count;
        if (active < degree_min) 
            degree_min = active;
    }

    /* Print simulation statistics */
    printf("+++ Page access summary\n    PID     Accesses        Faults         Replacements                        Attempts\n");
    for(int i = 0; i < n_processes; i++) 
        printf("    %-10d%-10d%-6d(%.2f%%)    %-6d(%.2f%%)    %3d + %3d + %3d + %3d  (%.2f%% + %.2f%% + %.2f%% + %.2f%%)\n",
               i,
               processes[i].page_access,
               processes[i].page_faults,
               100.0 * processes[i].page_faults / processes[i].page_access,
               processes[i].page_replacements,
               100.0 * processes[i].page_replacements / processes[i].page_access,
               processes[i].attempt1,
               processes[i].attempt2,
               processes[i].attempt3,
               processes[i].attempt4,
               100.0 * processes[i].attempt1 / (processes[i].page_replacements ? processes[i].page_replacements : 1),
               100.0 * processes[i].attempt2 / (processes[i].page_replacements ? processes[i].page_replacements : 1),
               100.0 * processes[i].attempt3 / (processes[i].page_replacements ? processes[i].page_replacements : 1),
               100.0 * processes[i].attempt4 / (processes[i].page_replacements ? processes[i].page_replacements : 1));
    printf("\n    Total     %-10d%-6d(%.2f%%)    %-6d(%.2f%%)    %d + %d + %d + %d  (%.2f%% + %.2f%% + %.2f%% + %.2f%%)\n",
           page_accesses,
           page_faults,
           100.0 * page_faults / page_accesses,
           page_replacements,
           100.0 * page_replacements / page_accesses,
           attempt1,
           attempt2,
           attempt3,
           attempt4,
           100.0 * attempt1 / (page_replacements ? page_replacements : 1),
           100.0 * attempt2 / (page_replacements ? page_replacements : 1),
           100.0 * attempt3 / (page_replacements ? page_replacements : 1),
           100.0 * attempt4 / (page_replacements ? page_replacements : 1));

    /* Free allocated memory for each process and global free frame list */
    for (int i = 0; i < n_processes; i++) 
    {
        free(processes[i].keys);
        free(processes[i].page_table);
        free(processes[i].history);
    }
    free(processes);
    free(free_frame_list);

    return 0;
}
