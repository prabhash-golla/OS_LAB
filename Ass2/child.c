#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>

int n;
pid_t child_pids[100];
int my_index;
int status = 1; // 1 = PLAYING, 0 = OUTOFGAME

// Signal handler for SIGUSR2 (ball thrown)
void handle_throw(int sig)
{
    if (status == 0) return; // Ignore if out of game

    int catch = rand() % 5 < 4; // 80% chance to catch

    if (catch) 
    {
        status = 2; // CATCH
        kill(getppid(), SIGUSR1); // Notify parent: caught
    } 
    else 
    {
        status = 3; // MISS
        kill(getppid(), SIGUSR2); // Notify parent: missed
    }
    return;
}

// Signal handler for SIGUSR1 (print status)
void handle_print(int sig) 
{
    if(my_index==0)
    {
        printf("|");
        fflush(stdout);
    }
    if (status == 1) 
    {
        printf("     ....    "); // .... Not Involved
    } 
    else if (status == 2) 
    {
        printf("    CATCH    "); // CATCH
        status = 1;
    }
    else if (status == 3) 
    {
        printf("     MISS    "); // MISS
        status = 0;
    }
    else 
    {
        printf("             "); // Blank for OUTOFGAME
    }
    fflush(stdout);
    if (my_index < n - 1) 
    {
        kill(child_pids[my_index + 1], SIGUSR1);
    }
    else 
    {
        pid_t dummy_pid = 0;
        while(dummy_pid==0)
        {
            FILE *file = fopen("dummycpid.txt", "r");
            fscanf(file, "%d", &dummy_pid);
            fclose(file);
        }
        FILE *file = fopen("dummycpid.txt", "w");
        fprintf(file, "%d", 0);
        fclose(file);
        //printf("%d ",dummy_pid);
        printf("|\n+");
        for(int i=0;i<n;i++) printf("-------------");
        printf("+\n");
        fflush(stdout);
        kill(dummy_pid, SIGINT);
    }
    return;
}

//Fuction for Printing the winner
void handle_win()
{
    if(status==1) 
    {
        printf("+++ Child %d : Yay! I am Winner\n",my_index+1);
    }
    exit(0);
}

int main() 
{
    srand(time(NULL) ^ getpid());

    signal(SIGUSR1, handle_print);
    signal(SIGUSR2, handle_throw);
    signal(SIGINT, handle_win);
    
    sleep(1); // Wait for parent to write childpid.txt

    FILE *file = fopen("childpid.txt", "r");
    fscanf(file, "%d", &n);
    for (int i = 0; i < n; i++) 
    {
        fscanf(file, "%d", &child_pids[i]);
        if (child_pids[i] == getpid()) my_index = i;
    }
    fclose(file);

    while (1) pause();
    return 0;
}
