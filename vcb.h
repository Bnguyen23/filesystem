#ifndef VCB_H
#define VCB_H

#include <sys/types.h>
#include "fsLow.h"

typedef struct VCB
{
	//details about the volume
	int magicNumber; //identifies volume in VCB
	int totalBytes; //size of the volume (in bytes)
	int blockSize; //size of volume's blocks (in bytes)
	int rootLBA; //LBA to root, is placed in the block after freeMap
	int mapLBA; //LBA to freeMap, default to block #1
	int mapCount; //number of blocks allocated for freeMap
	int totalBlocks; //number of total blocks
	int freeBlocks; //number of free blocks
	int usedBlocks; //number of used blocks


	//the following are only valid during runtime
	int cwdLBA; //holds LBA to cwd
	char * currPath; //string for holding pathname of cwd
} VCB;


extern VCB * vcb;
VCB * vcb;

int openVolume(uint64_t volumeSize, uint64_t blockSize);
int closeVolume();

void commitVCB();

int volumeRead(void * buffer, size_t bytes, int LBA);
int volumeWrite(void * buffer, size_t bytes, int LBA);

#endif