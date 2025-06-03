#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mfs.h"
#include "vcb.h"
#include "parsePath.h"
#include "freeMap.h"
#include "entry.h"

#include "printIndent.h"
#define DEBUG 0
#define DEBUG_DETAIL 0
#define DIRMAX_LEN		4096

//Global variable to tstore the currnet working directory
static entry current_working_directory;


//opening directory 
fdDir * fs_opendir(const char *pathname)
{
    //calling parsePath to get the entry structure with the provided path
    int dirIndex = parsePath((char*)pathname);

    //if there is no directory then it would return -1
    if(dirIndex == -1){
        perror("fs_opendir: No such file or directory");
        return NULL;
    }

    if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("fs_opendir: Opening directory\n");
	}

    //allocate memory for the fdDir struct
    fdDir *dir = (fdDir*)malloc(sizeof(fdDir));

    //intial directory entry positon
    dir->dirEntryPosition = 0;
    //set the directory starting location
    dir->directoryStartLocation = dirIndex;

    if(DEBUG)
	{
		printIndent();
		printf("fs_opendir: Directory opened\n");
        decrementIndent();
	}

    return dir;
}

//close directory and resets it's index 
int fs_closedir(fdDir *dirp)
{
    //check if dirp is NULL
    if(dirp == NULL){
        perror("fs_closedir: Invalid directory pointer");
        return -1;
    }

    if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("fs_closedir: Closing directory\n");
	}

    //calling parsePath to get the entry structure with the provided path
    int dirIndex = dirp->directoryStartLocation;

    //if there is no directory then it would return -1
    if(dirIndex == -1){
        perror("fs_closeddir: No such file or directory");
        return -1;
    }
    if(DEBUG)
	{
		printIndent();
		printf("fs_closedir: Directory closed\n");
        decrementIndent();
	}

    //free memory alloacted to fdDir struct 
    free(dirp);
    //return 0 to indicate the directory is successfully closed
    return 0;
}

// read the next directory entry from the directory
struct fs_diriteminfo *fs_readdir(fdDir *dirp){

    // if the index is invalid it would return -1
    if(dirp ->directoryStartLocation < 0){
        perror("fs_readdir: Invalid argument");
        return NULL;
    }

    if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("fs_readdir: Reading directory\n");
	}

	//iterate through directory
    for(int i = 0;; i++)
    {
		//get LBA to entry in directory
        int LBA = getEntryBlock(dirp->directoryStartLocation, dirp->dirEntryPosition + i);
        if(LBA == -1) return NULL;

		//check entry
        entry ent;
        volumeRead(&ent, sizeof(entry), LBA);

		//return entry info
        if(ent.type != FREE_ENTRY)
        {
            dirp->dirEntryPosition += i + 1;
            struct fs_diriteminfo *entryInfo = (struct fs_diriteminfo *)malloc(sizeof(struct fs_diriteminfo));


			//get cwd string and append file's name
			//copy the resulting string into entryInfo->d_name
			//this is needed because displayFiles() in fsshell.c calls fs_stat() which calls parsePath()


            if(DEBUG)
            {
                printIndent();
                printf("fs_readdir: Read entry [%s]\n", ent.name);
                decrementIndent();
            }

            //strncpy(entryInfo->d_name, ent.name, MAX_NAME_LEN);
            return entryInfo;
        }
    }
}

// gets the current working  directory
char* fs_getcwd(char *buffer, size_t size){
    // checks if the buffer is NULL or the size is greater than 0
    if(buffer == NULL || size == 0){
        perror("fs_getcwd: Invalid buffer or size");
        return NULL;
    }

    // check if the current path length is within the buffer size
    size_t cwd_length = strlen(vcb->currPath);
    if(cwd_length >= size){
        perror("fs_getcwd: buffer too small");
        return NULL;
    }

    // copy the current directory path to the buffer
    strncpy(buffer, vcb->currPath, size );

    // buffer null terminator
    buffer[size -1] = '\0';
    return buffer;
}

// Sets the current working directory
int fs_setcwd(char *pathname){
    char *pathToken;
    char *combinedPath;
    char *collapsedPath = (char *)calloc(1, strlen(vcb->currPath)); //256
    int token_index = 0;

    // // Concatenate cwd and user input
    // combinedPath = (char *)calloc(1, strlen(vcb->currPath) + strlen(pathname) + 2);
    // strcat(combinedPath, vcb->currPath);
    // strcat(combinedPath, "/");
    // strcat(combinedPath, pathname);

        // Check if the given pathname starts with '/', meaning it's an absolute path
    if (pathname[0] == '/') {
        // Allocate memory for combinedPath and copy the pathname
        combinedPath = (char *)calloc(1, strlen(pathname) + 1);
        strcpy(combinedPath, pathname);
    } else {
        // Concatenate cwd and user input for relative paths
        combinedPath = (char *)calloc(1, strlen(vcb->currPath) + strlen(pathname) + 2);
        strcat(combinedPath, vcb->currPath);
        strcat(combinedPath, "/");
        strcat(combinedPath, pathname);
    }


    // Tokenize the path
    // char **path_array = (char **)calloc(strlen(combinedPath), sizeof(char *));
     char **path_array = (char **)calloc((100), sizeof(char *));
    pathToken = strtok(combinedPath, "/");

    while(pathToken != NULL){
        path_array[token_index] = pathToken;
        pathToken = strtok(NULL, "/");
        token_index++;
    }

    // Collapse the path, handling ".." and "." path components
    collapsedPath =(char *)calloc(1, DIRMAX_LEN);
    for(token_index = 0; path_array[token_index] != NULL; token_index++){
        if(strcmp(path_array[token_index], "..") == 0){
            // when encountring "..", remove the current and previous path
            if(token_index > 0){
                path_array[token_index] = NULL;
                path_array[token_index - 1] = NULL;
            }
        } else if(strcmp(path_array[token_index], ".") == 0){
            // when encountering ".", remove the current path component
            path_array[token_index] = NULL;
        }
    }

    // Rebuild the collapsed path
    for(token_index = 0; path_array[token_index] != NULL; token_index++){
        if(path_array[token_index] != NULL){
            strcat(collapsedPath, "/");
            strcat(collapsedPath, path_array[token_index]);
        }
    }

    // convert the collapsed path to an LBA
    int new_cwdLBA = parsePath(collapsedPath);

    // Check if there is a directory
    if(new_cwdLBA == -1 || !fs_isDir(collapsedPath)){
        // Print an error message
        perror("fs_setcwd: No such file or directory");
        free(combinedPath);
        free(collapsedPath);
        free(path_array);
        return -1;
    }

    // Update the global working directory variable with the new entry
    vcb->cwdLBA = new_cwdLBA;
    strncpy(vcb->currPath, collapsedPath, DIRMAX_LEN);

    // free allocatd memory
    free(combinedPath);
    free(collapsedPath);
    free(path_array);
    return 0;
}



// deletes the specified file or directory
int fs_delete(char *filename){

    //get the LBA of the selected entry using parsePAth
    int selectedEntryLBA = parsePath(filename);
    
    // if directory is not found
    if(selectedEntryLBA == -1){
        perror("fs_delete: Directory not found");
        return -1;
    }

    //read the entry from disk
    entry selectedEntry;
    volumeRead(&selectedEntry,sizeof(entry),selectedEntryLBA);

    // free all blocks assigned to the file
    for(int i = 0;; i++){
        int blockLBA = getEntryBlock(selectedEntryLBA, i);
        if(blockLBA == -1) break;
        freeBlock(blockLBA, 1);
    }
    //checks if the tertiary table exists
    int tertiaryLBA = selectedEntry.data[TABLE_SIZE - 1].block;
	if(tertiaryLBA != 0){
        // Create array to hold secondary table LBA
		int tertiaryTable[INT_PER_BLOCK];

        //read the tertiary table from disk
        volumeRead(tertiaryTable, sizeof(tertiaryTable), tertiaryLBA);

        //Checks pimary tables and free them using freeblock
        for(int i = 0; i < INT_PER_BLOCK; i++){
            // freeBlock(selectedEntry.data[i].block, 1);

            //if a secondary table exists, free it using freeBlocks
            if(tertiaryTable[i] !=0){
                freeBlock(tertiaryTable[i],1);
            }
            else{
                break;
            }
        }

        //4. free the tertiary table block
        freeBlock(tertiaryLBA,1);
    }

    //set the etnry name to an empty string
    strcpy(selectedEntry.name, "\0");

    //set the entry type to free entry
    selectedEntry.type = FREE_ENTRY;

    //modif entry back to disk
    volumeWrite(&selectedEntry, sizeof(entry), selectedEntryLBA);

    return 0;
}