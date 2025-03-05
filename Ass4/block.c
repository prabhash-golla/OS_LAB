#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#define BLOCK_SIZE 3            // Defines the size of the block in a 3x3 Sudoku grid

// Global variables for file descriptors and block management
int Std_Out_Cpy;
int bn, rfd, wfd, rn1w, rn2w, cn1w, cn2w;
int B[BLOCK_SIZE][BLOCK_SIZE], A[BLOCK_SIZE][BLOCK_SIZE];

// Function to check row conflicts by sending a request to a neighbor process
int crc(int wfd, int row, int digit, int neighbor_fd) 
{
    close(1);
    dup(neighbor_fd);
    printf("r %d %d %d\n", row % BLOCK_SIZE, digit, wfd);
    close(1);
    dup(Std_Out_Cpy);
    int response;
    scanf("%d", &response);
    return response;
}

// Function to check column conflicts by sending a request to a neighbor process
int ccc(int wfd, int col, int digit, int neighbor_fd) 
{
    close(1);
    dup(neighbor_fd);
    printf("c %d %d %d\n", col % BLOCK_SIZE, digit, wfd);
    close(1);
    dup(Std_Out_Cpy);
    int response;
    scanf("%d", &response);
    return response;
}

// Function to display the Sudoku block
void draw_block() 
{
    printf("\033[H\033[J");         // Clears the screen
    for (int i = 0; i < BLOCK_SIZE; i++) 
    {
        printf("  +---+---+---+\n  ");
        for (int j = 0; j < BLOCK_SIZE; j++) 
        {
            if (B[i][j] == 0)
            {
                printf("|   ");             // Empty cell
            }
            else
            {
                printf("| %d ", B[i][j]);   // Display digit
            }
        }
        printf("|\n");
    }
    printf("  +---+---+---+\n");
}

//Add Num
void Add_Num(int cell,int digit)
{
    int row = cell / BLOCK_SIZE;
    int col = cell % BLOCK_SIZE;

    // Check if the cell is read-only (pre-filled)
    if (A[row][col] != 0) 
    {
        printf("Read-only cell");
        fflush(stdout);
        sleep(2);
        draw_block();
        return;
    }
    // Check for block conflict
    for (int i = 0; i < BLOCK_SIZE; i++) 
    {
        for (int j = 0; j < BLOCK_SIZE; j++) 
        {
            if (B[i][j] == digit) 
            {
                printf("Block conflict");
                fflush(stdout);
                sleep(2);
                draw_block();
                return;
            }
        }
    }
    // Check for row conflicts
    if (crc(wfd, row, digit, rn1w) || crc(wfd, row, digit, rn2w)) 
    {
        printf("Row conflict");
        fflush(stdout);
        sleep(2);
        draw_block();
        return;
    }
    // Check for column conflicts
    if (ccc(wfd, col, digit, cn1w) || ccc(wfd, col, digit, cn2w)) 
    {
        printf("Column conflict");
        fflush(stdout);
        sleep(2);
        draw_block();
        return;
    }
    // Place the digit into the block
    B[row][col] = digit;
    draw_block();
}

// Main function to handle process communication and block management
int main(int argc, char *argv[]) 
{
    if (argc != 8) 
    {
        fprintf(stderr, "Usage: %s blockno read_fd wfd rn1w rn2w cn1w cn2w\n", argv[0]);
        exit(1);
    }
    
    // Parse command line arguments
    bn = atoi(argv[1]);
    rfd = atoi(argv[2]);
    wfd = atoi(argv[3]);
    rn1w = atoi(argv[4]);
    rn2w = atoi(argv[5]);
    cn1w = atoi(argv[6]);
    cn2w = atoi(argv[7]);
    
    // Duplicate standard output for later restoration
    Std_Out_Cpy = dup(1);
    close(0);
    dup(rfd);
    
    printf("Block %d Ready\n", bn);
    char command;
    while (1) 
    {
        scanf(" %c", &command);
        switch (command) 
        {

            case 'n': // Initialize block
            {
                for (int i = 0; i < BLOCK_SIZE; i++) 
                {
                    for (int j = 0; j < BLOCK_SIZE; j++) 
                    {
                        scanf("%d", &A[i][j]);
                        B[i][j] = A[i][j];
                    }
                }
                draw_block();
                break;
            }

            case 'p': // Place digit
            {
                int cell, digit;
                scanf("%d %d", &cell, &digit);
                Add_Num(cell,digit);
                break;
            }

            case 'r': // Row check
            {
                int row, digit, rfd;
                scanf("%d %d %d", &row, &digit, &rfd);
                int p = 0;
                for (int j = 0; j < BLOCK_SIZE; j++) 
                {
                    if (B[row][j] == digit) 
                    {
                        p = 1;
                        break;
                    }
                }
                close(1);
                dup(rfd);
                printf("%d\n", p);
                close(1);
                dup(Std_Out_Cpy);
                break;
            }

            case 'c': // Column check
            {
                int col, digit, rfd;
                scanf("%d %d %d", &col, &digit, &rfd);
                int p = 0;
                for (int i = 0; i < BLOCK_SIZE; i++) 
                {
                    if (B[i][col] == digit) 
                    {
                        p = 1;
                        break;
                    }
                }
                close(1);
                dup(rfd);
                printf("%d\n", p);
                close(1);
                dup(Std_Out_Cpy);
                break;
            }

            case 'q': // Quit
            {
                printf("Bye...");
                fflush(stdout);
                sleep(2);
                exit(EXIT_SUCCESS);
            }

        }
    }
    return 0;
}