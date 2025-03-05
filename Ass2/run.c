#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

int main()
{
    for(int i=0;i<50;i++)
    {
        int k = fork();
        if(k==0)
        {
            execlp("./parent","./parent","11",NULL);
        }
        else
        {
            waitpid(k,NULL,0);
        }
    }
    return 0;
}