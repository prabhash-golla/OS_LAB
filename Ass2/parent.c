#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_CHILDREN 100

int n;                          // Number of children
pid_t child_pids[MAX_CHILDREN]; // Array to store child PIDs
pid_t dummy_pid;                // PID of dummy process
pid_t first_child;
int current_child = 0;          // Index of current child to throw the ball
int k;
int s=0;

// Signal handler for SIGUSR1 and SIGUSR2
void handle_signal(int sig) 
{
    if (sig == SIGUSR2) 
    {
        child_pids[current_child] = 0; // Mark as out
        k--;
    }
    
    current_child = (current_child + 1) % n;

    // Find next Child in the Game
    while(child_pids[current_child]==0) current_child = (current_child + 1) % n;
    s=1;
    return;
    
}

// Function to create dummy process
void create_dummy() 
{
    dummy_pid = fork();
    if (dummy_pid == 0) 
    {
        execl("./dummy","./dummy",NULL);
    }
    else
    {
        FILE *file = fopen("dummycpid.txt", "w");
        fprintf(file, "%d\n", dummy_pid);
        fclose(file);
    }
}

// Main function
int main(int argc, char *argv[]) 
{
    if (argc != 2) 
    {
        fprintf(stderr, "Usage: %s <number_of_children>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    n = atoi(argv[1]);
    if (n < 1 || n > MAX_CHILDREN) 
    {
        fprintf(stderr, "Invalid number of children.\n");
        exit(EXIT_FAILURE);
    }
    k = n;

    signal(SIGUSR1, handle_signal);
    signal(SIGUSR2, handle_signal);

    FILE *file = fopen("childpid.txt", "w");
    fprintf(file, "%d\n", n);

    // Create child processes
    for (int i = 0; i < n; i++) 
    {
        pid_t pid = fork();
        if (pid == 0) 
        {
            execl("./child", "./child", NULL);
            perror("execl failed");
            exit(EXIT_FAILURE);
        }
        child_pids[i] = pid;
        fprintf(file, "%d\n", pid);
    }

    fclose(file);

    printf("Parent: %d child processes created.\n",n);
    first_child = child_pids[0];                    // Store The PID of First Child

   
    printf("Parent: Waiting for child processes to read child database\n");
    sleep(2);                                       // Give time to read childpid.txt

    // Print numbers at Top of the Table
    for(int i=0;i<n;i++) printf("      %d      ",i+1);
    printf("\n+");

    // Print Lines Top of the table
    for(int i=0;i<n;i++) printf("-------------");
    printf("+\n");


    // Main game loop
    // Start the game by sending SIGUSR2 to the first child
    while (1)
    {
        if(k==1) break;                             // Break if only one child
        kill(child_pids[current_child], SIGUSR2);   // Throw ball to next child
        //if(!s) pause();
        s=0;
        create_dummy();                             // Create dummy process
        kill(first_child,SIGUSR1);                  // Call First Child for Printing
        waitpid(dummy_pid, NULL, 0);                // Wait for dummy process to exit
    }
    
    // Read All Child Pids Again
    file = fopen("childpid.txt", "r");
    fscanf(file, "%d", &n);
    for (int i = 0; i < n; i++) 
    {
        fscanf(file, "%d", &child_pids[i]);
    }
    fclose(file);  


    // Print numbers at Bottam of the Table
    for(int i=0;i<n;i++) printf("      %d      ",i+1);
    printf("\n");

    // Sending "SIGINT" to all the Childs
    for(int i=0;i<n;i++)
    {
        kill(child_pids[i],SIGINT);
    }
}
