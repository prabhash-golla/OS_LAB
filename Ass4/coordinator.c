#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include "boardgen.c"

#define BLOCKS 9                                             // Number of blocks in the Sudoku grid

// Function to print help menu with supported commands
void print_help() 
{
    printf("\nCommands Supported\n");
    printf("\tn - Start new game\n") ;
    printf("\tp b c d - Place digit d [1-9] at cell c [0-8] of block b [0-8]\n");
    printf("\ts - Show S\n");
    printf("\th - Print this help message\n");
    printf("\tq - Quit\n\n");

    // Print block and cell numbering scheme for user reference
    printf("Numbering Scheme for block and cells\n");
    for(int i=0;i<3;i++)
    {
        printf("\t+---+---+---+\n\t");
        for (int j = 0; j < 3; j++)
        {
            printf("| %d ", 3*i+j);
        }
        printf("|\n");
    }
    printf("\t+---+---+---+\n\n");
    fflush(stdout);
}

int main() 
{
    pid_t pids[BLOCKS];             // Store process IDs for each block
    int pipes[BLOCKS][2];           // Pipes for inter-process communication
    int A[BLOCKS][BLOCKS];          // Sudoku grid (A) to store Original state
    int S[BLOCKS][BLOCKS];          // Sudoku grid (S) to store Solution state
    memset(S,0,sizeof(S));          // initialaise S with 0's

    // Creating pipes for each block
    for (int i = 0; i < BLOCKS; i++)
    {
        int fd[2];
        if(pipe(fd)<0)              // Check if pipe creation fails
        {
            printf("Pipe Creation Failure\n");
            exit(EXIT_FAILURE);
        }
        pipes[i][0]=fd[0];          // Read end of pipe
        pipes[i][1]=fd[1];          // Write end of pipe
    }   

    // Fork child processes for each block
    for(int i=0;i<BLOCKS;i++)
    {
        if((pids[i]=fork())<0)      // Check if fork fails
        {
            printf("Error Fork \n");
            exit(EXIT_FAILURE);
        }

        if(!pids[i])                // Child process
        {
            // Compute row and column neighbors
            int rn1 = 3*(i/3)+(i+1)%3;
            int rn2 = 3*(i/3)+(i+2)%3;
            int cn1 = (i+3)%9;
            int cn2 = (i+6)%9;
            
            // Convert variables to strings for execl arguments
            char Block[4], rfd[4], wfd[4];
            char rn1w[4], rn2w[4], cn1w[4], cn2w[4];

            int x = 950 + (i % 3) * 300;    // X-coordinate for window positioning
            int y = 100 + (i / 3) * 300;    // Y-coordinate for window positioning
            char Shape[32];                 // String to store terminal geometry
            char Title[10];                 // String to store window title

            // Convert integers to strings
            sprintf(Block, "%d", i);
            sprintf(rfd, "%d", pipes[i][0]);
            sprintf(wfd, "%d", pipes[i][1]);
            sprintf(rn1w, "%d", pipes[rn1][1]);
            sprintf(rn2w, "%d", pipes[rn2][1]);
            sprintf(cn1w, "%d", pipes[cn1][1]);
            sprintf(cn2w, "%d", pipes[cn2][1]);

            sprintf(Shape, "17x8+%d+%d", x, y);          // Window size and position
            sprintf(Title, "Block %d", i);               // Window title

            // Execute `xterm` to launch separate block windows
            execl("/usr/bin/xterm", "xterm","-T", Title,"-fa", "Monospace","-fs", "15","-geometry", Shape,"-bg", "black","-fg", "white","-e", "./block",Block, rfd, wfd,rn1w, rn2w, cn1w, cn2w,NULL);
            
            // If execl fails, print error
            perror("exec failed\n");
            exit(EXIT_FAILURE);
        }
    }
    print_help();                   // Intial Display of  help menu
    int Std_Out_Cpy = dup(1);       // Backup stdout file descriptor
    int f = 1 ;
    while (1)
    {
        char c;
        int b,co,d;
        printf("Foodoku> ");
        scanf(" %c",&c);            // Read user input command

        switch (c) {
            case 'h':               // Help command
            {
                print_help();        
                break;
            } 
            case 'n':               // Start new game
            {
                f =0;
                newboard(A, S);    // Generate new board(A) & Solution(S)              
                close(1);// Close standard output

                // Send initial game board to each block process
                for (int i = 0; i < BLOCKS; i++) 
                {
                    dup(pipes[i][1]);       // Redirect stdout to pipe
                    printf("n ");           // Send new game signal
                    int block_row = (i / 3) * 3;
                    int block_col = (i % 3) * 3;

                    // Send the 3x3 block values
                    for (int r = 0; r < 3; r++) 
                    {
                        for (int c = 0; c < 3; c++) 
                        {
                            printf("%d ", A[block_row + r][block_col + c]);
                        }
                    }
                    fflush(stdout);
                    close(1);
                }
                dup(Std_Out_Cpy);           // Restore stdout
                break;
            }   
            case 'p':               // Place a number
            {
                scanf("%d %d %d", &b, &co, &d);
                if(f==1) 
                {   
                    printf("Please ,Start a Game first\n");
                    break;
                }
                if(f==2)
                {
                    printf("Please ,Start a New Game first\n");
                    break;
                }
                if (b < 0 || b >= BLOCKS || co < 0 || co >= BLOCKS || d < 1 || d > BLOCKS) 
                {
                    printf("Invalid input ranges\n");
                    break;
                }
                close(1);
                dup(pipes[b][1]);           // Redirect stdout to block pipe
                printf("p %d %d\n", co, d); // Send place command
                fflush(stdout);
                close(1);
                dup(Std_Out_Cpy);
                break;
            }   
            case 's':              // Show Solution board 
            {
                if(f==1) 
                {   
                    printf("Please ,Start a game first\n");
                    break;
                }
                f=2;
                for (int i = 0; i < BLOCKS; i++)
                {
                    close(1); 
                    dup(pipes[i][1]);   // Redirect stdout to pipe
                    printf("n ");
                    int block_row = (i / 3) * 3;
                    int block_col = (i % 3) * 3;
                    for (int r = 0; r < 3; r++) 
                    {
                        for (int c = 0; c < 3; c++) {

                            printf("%d ",S[block_row + r][block_col + c]);
                        }
                    }
                    fflush(stdout);
                    close(1);
                    dup(Std_Out_Cpy);
                }
                break;
            }   
            case 'q':           // Quit game
            {
                for (int i = 0; i < BLOCKS; i++) 
                {
                    close(1);
                    dup(pipes[i][1]);
                    printf("q\n");      // Send quit signal to each block
                    fflush(stdout);
                    close(1);
                    dup(Std_Out_Cpy);
                }
                for (int i = 0; i < BLOCKS; i++) 
                {
                    wait(NULL);        // Wait for all child processes to terminate
                }
                exit(0);
            }   
            default:
                printf("Invalid command. Use 'h' for help.\n");
        }

    }
    
    return 0;
}
