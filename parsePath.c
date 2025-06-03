#include "parsePath.h"

#include <stdlib.h>
#include <string.h>
#include "entry.h"
#include "vcb.h"
#include "mfs.h"

#define FILE_SEP "/"

#include "printIndent.h"
#define DEBUG 0
#define DEBUG_DETAIL 0

//takes a pathname and returns LBA if pathname exists, returns -1 if fails
int parsePath(const char * path)
{
    if(DEBUG) 
    {
        incrementIndent();
        printIndent();
        printf("parsePath: Parsing -> [%s]\n", path);
    }
    
    //deep copy the path
    char * string = malloc(strlen(path) + 1);
    if(string == NULL)
    {
        perror("\n*** failed to malloc() \"string\" in parsePath() ***\nerror");
        exit(137);
    }
    strcpy(string, path);

    //determine if path starts from root
    int LBA;
    int parentLBA = -1;
    if(string[0] == '/') 
    {
        LBA = vcb->rootLBA;
        parentLBA = vcb->rootLBA; //so root's ".." gets updated
    }
    else LBA = vcb->cwdLBA;


    //parse the path
    char * token = strtok(string, FILE_SEP);
    while(token != NULL)
    {
        //check current position in path
        entry ent;
        volumeRead(&ent, sizeof(entry), LBA);
        if(ent.type != DIR_ENTRY)
        {
            //is not a directory, can't continue parse
            free(string);
            return -1;
        }

        if(DEBUG_DETAIL)
        {
            printIndent();
            printf("parsePath: Copy down \"dot\" DIR info\n");
        }

        //setting self info to "." directory
        volumeRead(&ent, sizeof(entry), LBA);
        memset(ent.name, 0, MAX_NAME_LEN);
        strcpy(ent.name, ".");
        volumeWrite(&ent, sizeof(entry), getEntryBlock(LBA, 0));
		//setting parent directory info to ".." directory
        if(parentLBA != -1)
        {
            volumeRead(&ent, sizeof(entry), parentLBA);
            memset(ent.name, 0, MAX_NAME_LEN);
            strcpy(ent.name, "..");
            volumeWrite(&ent, sizeof(entry), getEntryBlock(LBA, 1));
        }


        if(DEBUG)
        {
            printIndent();
            printf("parsePath: token = [%s]\n", token);
            printIndent();
            printf("parsePath: DIR LBA = %d\n", LBA);
        }

        //iterate through entries in directory
        for(int i = 0;; i++)
        {
            //get an LBA to an entry
            int blobLBA = getEntryBlock(LBA, i);
            if(blobLBA == -1)
            {
                if(DEBUG)
                {
                    printIndent();
                    printf("parsePath: LBA found -> %d\n", -1);
                    decrementIndent();
                }

                //end of directory reached
                free(string);
                return -1;
            }

            //read the directory entry
            volumeRead(&ent, sizeof(entry), blobLBA);

            if(DEBUG_DETAIL)
            {
                printIndent();
                printf("\t%06d: [%s]\n", i, ent.name);
            }

            if(strcmp(ent.name, token) == 0)
            {
                parentLBA = LBA;
                //matching entry found
                LBA = blobLBA;
                break;
            }
        }

        //get next token
        token = strtok(NULL, "/");
    }

    if(DEBUG)
    {
        printIndent();
        printf("parsePath: LBA found -> %d\n", LBA);
        decrementIndent();
    }

    //cleanup & return
    free(string);
    string = NULL;
    return LBA;
}
