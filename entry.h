#ifndef ENTRY_H
#define ENTRY_H

#include <stdio.h>
#include <time.h>

#define MAX_NAME_LEN 256 //assignment says max name is 255 + 1 for the '\0'
#define MAX_AUTHOR_LEN 36 //linux username has 32 char limit, but 33 is inconvenient for padding
#define TABLE_SIZE 22 //remaining block space is used for extents
#define ROOT_DIR_NAME "root"
#define SYSTEM_NAME "system"

#define FREE_ENTRY 0x00
#define DIR_ENTRY 0x01
#define FILE_ENTRY 0x02

#define INT_PER_BLOCK (vcb->blockSize / sizeof(int))
#define EXT_PER_BLOCK (vcb->blockSize / sizeof(extent))

typedef struct extent //size = 8 bytes
{
    int block; //LBA to start of allocated blocks
    int count; //number of continuous blocks
} extent;

typedef struct entry //size = 512 bytes
{
    char name[MAX_NAME_LEN]; //file's name
    char author[MAX_AUTHOR_LEN]; //file's author (type fills in to prevent struct padding)
    unsigned int type; //entry's data is more entries (is directory) or raw data (is file)
    unsigned int permission; //file's permissions
    unsigned int blockCount; //number of blocks file takes
    size_t size; //file's size (in bytes)
    extent data[TABLE_SIZE]; //primary table, last is pointer to index block for more extent tables
    time_t created; //time when created, epoch
    time_t modified; //time last changed, epoch
    time_t accessed; //time last opened, epoch
} entry;

void freeEntry(int entryLBA);
int growEntry(int entryLBA, int count);
int shrinkEntry(int entryLBA, int count);
int getEntryBlock(int entryLBA, int blockOffset);
int newFile(int destLBA, const char * name, const char * author);
int newDirectory(int destLBA, int parentLBA, const char * name, const char * author);
int getFreeEntry(int dirLBA);
void printEntry(int LBA);

#endif
