#include "vcb.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "entry.h"
#include "freeMap.h"

#define VCB_MAGIC_NUMBER 0xDCBA4321

static void newVolume(uint64_t volumeSize, uint64_t blockSize);
static void newVCB(size_t volumeSize, size_t blockSize);

#include "printIndent.h"

#define DEBUG 0
#define DEBUG_DETAIL 0
//sets up the system for interacting with the volume file, returns 0 if successful
//fileName = name of volume's file
//totalSize = size of the volume (in bytes)
//blockSize = size of blocks in volume (in bytes)
int openVolume(uint64_t volumeSize, uint64_t blockSize)
{
	//check parameters
	if(vcb != NULL) 
	{
		printf("*** openVolume() failed because another volume is already open ***\n");
		return -1;
	}
	//ensure fsLow's minSize supports our entry struct
	if(MINBLOCKSIZE < sizeof(entry))
	{
		printf("*** openVolume() failed because fsLow's block size is too small ***\n");
		return -1;
	}
	if(blockSize < MINBLOCKSIZE)
	{
		printf("*** openVolume() failed because the block size passed in was too small ***\n");
		return -1;
	}
	// //freeMap has to reserve at least 1 block, so minSize = 2,097,152 bytes for freeMap to use 100%
	// if((volumeSize / blockSize) < (blockSize * 8))
	// {
	// 	printf("*** openVolume() failed because the volume size was too small ***\n");
	// 	return -1;
	// }


	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("openVolume: Opening Volume w/ params:\n");

		printIndent();
		printf("\tvolumeSize: %ld bytes\n", volumeSize);
		printIndent();
		printf("\tblockSize: %ld bytes\n", blockSize);

		printIndent();
		printf("openVolume: Getting VCB\n");
	}

	//setup the VCB
	vcb = (VCB *)malloc(sizeof(VCB));
	if(vcb == NULL)
	{
		perror("\n*** failed to malloc() \"vcb\" in openVolume() ***\nerror");
		exit(137);
	}
	//can't use volumeRead() yet bc VCB is uninitialized so have to manually LBAread()
	char iobuffer[blockSize];
	LBAread(iobuffer, 1, 0);
	memcpy(vcb, iobuffer, sizeof(VCB));


	if(DEBUG) 
	{
		printIndent();
		printf("openVolume: Checking magic number: %x\n", vcb->magicNumber);
	}
	
	//check if volume is formatted
	if(vcb->magicNumber == VCB_MAGIC_NUMBER)
	{	
		if(DEBUG)
		{
			printIndent();
			printf("openVolume: Volume is formatted\n");
			printIndent();
			printf("openVolume: Getting existing freeMap from Volume\n");
		}

		//get freeMap from volume
		initMap(vcb->totalBlocks);
		volumeReadMap();

		if(DEBUG)
		{
			printIndent();
			printf("openVolume: freeMap obtained\n");
		}
	}
	//initialize volume
	else 
	{
		if(DEBUG)
		{
			printIndent();
			printf("openVolume: Volume is not formatted\n");
		}

		newVolume(volumeSize, blockSize);

		if(DEBUG) 
		{
			printIndent();
			printf("openVolume: Finished volume formatting\n");
		}
	}


	if(DEBUG)
	{
		printIndent();
		printf("openVolume: Initializing current working directory\n");
	}

	//initialize current working directory
	vcb->cwdLBA = vcb->rootLBA;
	vcb->currPath = malloc(MAX_NAME_LEN);
	if(vcb->currPath == NULL)
	{
		perror("\n*** failed to malloc() \"vcb->currPath\" in openVolume() ***\nerror");
		exit(137);
	}
	strcpy(vcb->currPath, "/");

	if(DEBUG)
	{
		printIndent();
		printf("openVolume: CWD initialized -> %d\n", vcb->cwdLBA);
		printIndent();
		printf("openVolume: Volume open finished\n");
		decrementIndent();
	}

	return 0;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//closes the volume file and frees the memory allocated for the VCB and free block
int closeVolume()
{
	if(vcb != NULL)
	{
		printf("System exiting\n");
		
		//free the freeMap
		freeFreeMap();

		//free the VCB
		free(vcb->currPath);
		free(vcb);
		vcb = NULL;
	}
	else
	{
		printf("Volume not open\n");
		return -1;
	}

	return 0;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//creates a new volume with specified size and block size
//totalSize = size of the volume
//blockSize = size of blocks in volume
static void newVolume(uint64_t volumeSize, uint64_t blockSize)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("newVolume: Creating new volume of size %ld with block size %ld\n", volumeSize, blockSize);
		printIndent();
		printf("newVolume: Creating new VCB\n");
	}

	//create VCB and freeMap
	newVCB(volumeSize, blockSize);

	if(DEBUG)
	{
		printIndent();
		printf("newVolume: Creating new freeMap\n");
	}

	initMap(vcb->totalBlocks);

	if(DEBUG)
	{
		printIndent();
		printf("newVolume: Marking %d blocks for VCB, freeMap, & root\n", vcb->mapCount + 2);
	}

	markBlock(0, vcb->mapCount + 2); //+2 is for VCB @ LBA=0 & root @ LBA=n

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("newVolume: Writing type \"DIR_ENTRY\" to root's LBA\n");
	}

	//set DIR entry type at rootLBA so newDirectory() works
	entry ent;
	memset(&ent, 0, sizeof(entry));
	ent.type = DIR_ENTRY;
	volumeWrite(&ent, sizeof(entry), vcb->rootLBA);


	if(DEBUG)
	{
		printIndent();
		printf("newVolume: Creating root directory at LBA %d\n", vcb->rootLBA);
	}

	//make root
	newDirectory(vcb->rootLBA, vcb->rootLBA, ROOT_DIR_NAME, SYSTEM_NAME);

	if(DEBUG)
	{
		printIndent();
		printf("newVolume: Volume creation finished\n");
		decrementIndent();
	}
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//performs size calculations and sets data in VCB
//volumeSize = size of the volume
//blockSize = size of blocks in volume
static void newVCB(size_t volumeSize, size_t blockSize)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("newVCB: Creating new VCB for volume size %ld with block size %ld\n", volumeSize, blockSize);
	}

	//calculate volume storage
	int totalBlocks = volumeSize / blockSize;
	int bitsPerBlock = blockSize * 8;
	int freeMapUsage = (totalBlocks + (bitsPerBlock - 1)) / bitsPerBlock;

	//create VCB
	vcb->magicNumber = VCB_MAGIC_NUMBER;
	vcb->totalBytes = volumeSize;
	vcb->blockSize = blockSize;
	vcb->rootLBA = freeMapUsage + 1;
	vcb->mapLBA = 1;
	vcb->mapCount = freeMapUsage;
	vcb->totalBlocks = totalBlocks;
	vcb->freeBlocks = totalBlocks;
	vcb->usedBlocks = 0;

	if(DEBUG_DETAIL)
	{
		incrementIndent();
		printIndent();
		printf("volume = %d total blocks\n", totalBlocks);
		printIndent();
		printf("freeMap = %d blocks used\n", freeMapUsage);
		decrementIndent();

		printIndent();
		printf("newVCB: Committing the new VCB\n");
	}

	//write to volume
	commitVCB();

	if(DEBUG)
	{
		printIndent();
		printf("newVCB: Finished creating the new VCB\n");
		decrementIndent();
	}
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//writes the vcb to disk at LBA 0
void commitVCB()
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("vcb: commitVCB() was called\n");
	}

	volumeWrite(vcb, sizeof(VCB), 0);

	if(DEBUG)
	{
		printIndent();
		printf("vcb: VCB committed\n");
		decrementIndent();
	}
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//takes a buffer and reads requested amount into it from the volume starting at the passed LBA
//buffer = pointer to buffer for copying data into
//bytes = the # of bytes you want to read into the buffer
//LBA = starting block of read request
int volumeRead(void * buffer, size_t bytes, int LBA)
{
	if(DEBUG_DETAIL)
	{
		incrementIndent();
		printIndent();
		printf("vcb: volumeRead() request for %ld bytes from LBA %d into %p\n", bytes, LBA, buffer);
	}

	//ensure request is valid
	if((LBA < 0) || (LBA >= vcb->totalBlocks)) return 0;
	if(bytes <= 0) return 0;
	int cap = vcb->totalBytes - (LBA * vcb->blockSize);
	if(bytes > cap) bytes = cap;


	//directly read blocks into user's buffer
	int count = bytes / vcb->blockSize;
	if(count > 0)
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("vcb: Reading %d blocks directly into buffer\n", count);
		}

		LBAread(buffer, count, LBA);

		//increment positions
		LBA += count;
		buffer += count * vcb->blockSize;
	}


	//copy block into iobuffer then remaining into user's buffer
	count = bytes % vcb->blockSize;
	if(count > 0)
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("vcb: Reading single block at LBA %d\n", LBA);
		}

		char iobuffer[vcb->blockSize];
		LBAread(iobuffer, 1, LBA);
		memcpy(buffer, iobuffer, count);

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("vcb: Copied %d bytes into %p\n", count, buffer);
		}
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("vcb: Read total of %ld bytes\n", bytes);
		printIndent();
		printf("vcb: Finished volumeRead()\n");
		decrementIndent();
	}

	//return # of bytes read
	return bytes;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//takes a buffer and writes requested amount from it to volume starting at the passed LBA
//buffer = pointer to buffer for copying data from
//bytes = the # of bytes you want to write from the buffer
//LBA = starting block of write request
int volumeWrite(void * buffer, size_t bytes, int LBA)
{
	if(DEBUG_DETAIL)
	{
		incrementIndent();
		printIndent();
		printf("vcb: volumeWrite() request to write %ld bytes from %p into LBA %d\n", bytes, buffer, LBA);
	}

	//ensure request is valid
	if((LBA < 0) || (LBA >= vcb->totalBlocks)) return 0;
	if(bytes <= 0) return 0;
	int cap = vcb->totalBytes - (LBA * vcb->blockSize);
	if(bytes > cap) bytes = cap;


	//directly write blocks from user's buffer to volume
	int count = bytes / vcb->blockSize;
	if(count > 0)
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("vcb: Writing %d blocks directly from buffer\n", count);
		}

		LBAwrite(buffer, count, LBA);

		//increment positions
		LBA += count;
		buffer += count * vcb->blockSize;
	}


	//copy remaining in buffer into iobuffer then write to volume
	count = bytes % vcb->blockSize;
	if(count > 0)
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("vcb: Copying %d bytes from %p\n", count, buffer);
		}

		char iobuffer[vcb->blockSize];
		memset(iobuffer, 0, vcb->blockSize);
		memcpy(iobuffer, buffer, count);
		LBAwrite(iobuffer, 1, LBA);

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("vcb: Wrote to block at LBA %d\n", LBA);
		}
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("vcb: Wrote total of %ld bytes\n", bytes);
		printIndent();
		printf("vcb: finished volumeWrite()\n");
		decrementIndent();
	}

	//return # of bytes written
	return bytes;
}
