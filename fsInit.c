/**************************************************************
* Class:  CSC-415-0# Fall 2021
* Names: 
* Student IDs:
* GitHub Name:
* Group Name:
* Project: Basic File System
*
* File: fsInit.c
*
* Description: Main driver for file system assignment.
*
* This file is where you will start and initialize your system
*
**************************************************************/


#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>


#include "fsLow.h"
#include "mfs.h"
#include "vcb.h"


int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize)
{
	printf ("Initializing File System with %ld blocks with a block size of %ld\n", numberOfBlocks, blockSize);

	int retVal = openVolume(numberOfBlocks * blockSize, blockSize);
	if(retVal != 0) return retVal;
	
	return 0;
}
	
	
void exitFileSystem ()
{
	printf ("System exiting\n");
	closeVolume();
}