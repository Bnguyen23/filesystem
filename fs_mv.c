#include <string.h>
#include <stdlib.h>

#include "parsePath.h"
#include "splitPath.h"
#include "entry.h"
#include "vcb.h"
#include "mfs.h"
//takes two paths and copies entry from srcPath to destPath
int fs_mv(const char * srcPath, const char * destPath){
    char * path = malloc(strlen(srcPath));
    if(path == NULL){
        perror("could not malloc path\n");
        exit(137);
    }
    char * srcName = malloc(strlen(srcPath));
    if(path == NULL){
        perror("could not malloc name\n");
        exit(137);
    }
    char * destName = malloc(strlen(srcPath));
    if(path == NULL){
        perror("could not malloc name\n");
        exit(137);
    }


    //parsePath takes care of relative calls
    splitPathname(destPath,path,srcName);    
    int destLBA= parsePath(path);
    splitPathname(srcPath,path,destName);
    int srcLBA = parsePath(path);
    
    if(srcLBA == -1){
        printf("src file does not exist\n");
        return -1;
    }
    entry srcEntry;
	volumeRead(&srcEntry, sizeof(entry), srcLBA);

    entry destEntry;
    if(destLBA != -1){
	    volumeRead(&destEntry, sizeof(entry), destLBA); 
    }

    //checks if entry is directory and parentLBA is valid
    if(srcEntry.type = FILE_ENTRY){

        //if file already exists, overwrite & free srcEntry
        if(destLBA != -1){
            memcpy(&destEntry, &srcEntry,sizeof(entry));            
            volumeWrite(&destEntry, sizeof(entry), destLBA);
        }
        //otherwise create newFile
        else{
            int destIndex = getFreeEntry(destLBA);
            newFile(destIndex, srcEntry.name, srcEntry.author);
            memcpy(&destEntry, &srcEntry,sizeof(entry));            
            volumeWrite(&destEntry, sizeof(entry), destIndex);
        }
        freeEntry(srcLBA);
    }
    //if entry is not a file, must be a directory
    else{
        entry * srcDir;
        volumeRead(srcDir, sizeof(entry), destLBA);

        entry * destDir;
        volumeRead(destDir, sizeof(entry), destLBA);
        //if directory already exists, overwrite
        if(destLBA != -1){
            memcpy(destDir, srcDir,sizeof(entry));            
            volumeWrite(destDir, sizeof(entry), destLBA);
        }
        //otherwise create newFile
        else{
            int destLBA = getFreeEntry(destLBA);
            newFile(destLBA, srcEntry.name, srcEntry.author);
            memcpy(&destEntry, &srcEntry,sizeof(entry));            
            volumeWrite(&destEntry, sizeof(entry), destLBA);
        }
        freeEntry(srcLBA);
    }

    free(path);
    free(srcName);
    free(destName);
    path = NULL;
    srcName = NULL;
    destName = NULL;
}