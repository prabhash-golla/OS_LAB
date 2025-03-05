#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

void handle_signal(int sig) 
{
    exit(0); // Exit when SIGINT is received
}

int main() 
{
    signal(SIGINT, handle_signal);
    while (1) 
    {
        pause(); // Wait for SIGINT
    }
    return 0;
}
