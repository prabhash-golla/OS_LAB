#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <linux/limits.h>

typedef struct 
{
    unsigned int uid;
    char Owner[256];
} UID;

UID *table = NULL;
size_t tableSize = 0;

int number_of_files;

void loadUIDTable(char *passwd_file)
{
    FILE *fp = fopen(passwd_file, "r");
    if (!fp) 
    {
        printf("Failed to open %s",passwd_file);
        perror("");
        exit(EXIT_FAILURE);
    }
    
    char line[1024];
    size_t count = 0;
    while (fgets(line, sizeof(line), fp) != NULL)
    count++;
    
    rewind(fp);
    table = malloc(count * sizeof(UID));
    if (!table) 
    {
        perror("Memory allocation error");
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    
    tableSize = 0;
    while (fgets(line, sizeof(line), fp) != NULL) 
    {
        char *token = strtok(line, ":");
        if (!token) continue;

        strncpy(table[tableSize].Owner, token, sizeof(table[tableSize].Owner) - 1);
        table[tableSize].Owner[sizeof(table[tableSize].Owner)-1] = '\0';

        token = strtok(NULL, ":");
        if (!token) continue;

        token = strtok(NULL, ":");
        if (!token) continue;

        table[tableSize].uid = (unsigned int) atoi(token);
        tableSize++;
    }
    fclose(fp);
}

char *getOwner(unsigned int uid) 
{
    for (size_t i = 0; i < tableSize; i++) if (table[i].uid == uid)return table[i].Owner;
    return "UNKNOWN";
}

int has_extension(char *filename, char *extension) 
{
    size_t filename_len = strlen(filename);
    size_t extension_len = strlen(extension);
    
    if (filename_len <= extension_len + 1) return 0;
    
    char *dot = filename + filename_len - extension_len - 1;
    if (*dot != '.') return 0;
    
    return (strcmp(dot + 1, extension) == 0);
}

void searchInDir(char *dirname,char* extension)
{
    DIR* dir = opendir(dirname);
    if(dir==NULL)
    {
        printf("Error opening directory '%s'\n", dirname);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) 
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);

        struct stat file;
        if (lstat(path, &file) == -1) 
        {
            fprintf(stderr, "Error accessing file '%s'\n", path);
            continue;
        }
        
        if (S_ISDIR(file.st_mode)) searchInDir(path, extension);
        else if(S_ISREG(file.st_mode) && has_extension(entry->d_name, extension))
        {
            number_of_files++;
            printf("%-5d : %-20s %-10ld  %s\n", number_of_files, getOwner(file.st_uid), (long)file.st_size, path);
        }
    }

}


int main(int argc,char *argv[])
{
    if(argc<3)
    {
        printf("Usage: %s <directory> <extension>\n", argv[0]);
        exit(EXIT_FAILURE);
    }  
    number_of_files = 0;

    loadUIDTable("/etc/passwd");

    printf("%-5s : %-20s %-10s %s\n", "NO", "OWNER", "SIZE", "NAME");
    printf("--      ------               -----      ----\n");
    
    searchInDir(argv[1],argv[2]);

    printf("+++ %d files match the extension %s\n", number_of_files, argv[2]);

    free(table);

    return 0;
}