#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mfs.h"
#include "parsePath.h"
#include "entry.h"
#include "vcb.h"


#include "printIndent.h"
#define DEBUG 0
#define DEBUG_DETAIL 0

//takes path and fills in function passed buffer
int fs_stat(const char *path, struct fs_stat *buf)
{
    //check path exists
    int index = parsePath(path);
    if(index == -1){
        return -1;
    }

    if(DEBUG)
    {
        incrementIndent();
        printIndent();
        printf("fs_stat: Getting File Info\n");
    }

    //reading buffer from volume
    entry * buffer  = malloc(sizeof(struct entry));
    if(buffer == NULL){
        perror("||Could not malloc buffer ||");
        exit(1);
    }
    volumeRead(buffer, sizeof(struct entry), index);

    //fills in data fields found in "mfs.h"
    buf->st_accesstime = buffer->accessed;
    buf->st_size= buffer->size;
    buf->st_createtime = buffer->created;
    buf->st_modtime = buffer->modified;
    buf->st_blocks = buffer->size/vcb->blockSize;
    strcpy(buf->name, buffer->name);
    strcpy(buf->author, buffer->author);
    buf->type = buffer->type;


    if(DEBUG)
    {
        printIndent();
        printf("fs_stat: Read info from disk\n");
        decrementIndent();
    }


    free(buffer);
    buffer = NULL;
    return 0;
}