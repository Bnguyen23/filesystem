#include "freeMap.h"

#include <stdlib.h>

#include "vcb.h"
#include "entry.h"


#define INT_BIT_COUNT (sizeof(unsigned int) * 8) //32
#define MAP_BYTE_SIZE mapSize * sizeof(unsigned int) //~5 blocks


static unsigned int * freeMap;
static int mapSize; //is 610 for 19520 blocks


#include "printIndent.h"


#define DEBUG 0
#define DEBUG_DETAIL 0
//handles the malloc for volume's freeMap
//int totalBlocks = the total # of blocks in the volume
void initMap(int totalBlocks)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("initMap: Initializing map for %d blocks\n", totalBlocks);
	}

	mapSize = (totalBlocks + (INT_BIT_COUNT - 1)) / INT_BIT_COUNT;
	freeMap = calloc(mapSize, sizeof(unsigned int));
	if(freeMap == NULL)
	{
		perror("*** failed to malloc freeMap ***\nerror");
		exit(137);
	}

	if(DEBUG)
	{
		printIndent();
		printf("initMap: Map created with %d integers\n", mapSize);
		decrementIndent();
	}
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//free()'s the memory held by freeMap
void freeFreeMap()
{
	if(freeMap != NULL)
	{
		free(freeMap);
		freeMap = NULL;
		mapSize = 0;
	}
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//reads map from volume, requires that volume is open
void volumeReadMap()
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("volumeReadMap: Getting map from volume @ LBA %d\n", vcb->mapLBA);
	}

	volumeRead(freeMap, MAP_BYTE_SIZE, vcb->mapLBA);

	if(DEBUG)
	{
		printIndent();
		printf("volumeReadMap: freeMap read from volume, %d / %d blocks used\n", vcb->usedBlocks, vcb->totalBlocks);
		decrementIndent();
	}
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
// writes the freeMap to disk, requires that volume is open
void commitMap()
{
	if(DEBUG_DETAIL)
	{
		incrementIndent();
		printIndent();
		printf("commitMap: Commiting freeMap\n");
	}

	volumeWrite(freeMap, MAP_BYTE_SIZE, vcb->mapLBA);

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("commitMap: Map committed\n");
		decrementIndent();
	}
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//used by blockAlloc() to mark the according blocks in the freeMap as used
//returns: # of contiguous blocks starting from LBA, should be: 1 <= x <= count
//returns: -1 on bad parameters
int markBlock(int LBA, int count)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("markBlock: Request to mark (%d blocks) starting at (LBA %d)\n", count, LBA);
	}

	//check parameters
	if((LBA < 0) || (LBA >= vcb->totalBlocks)) 
	{
		if(DEBUG)
		{
			printIndent();
			printf("markBlock: Failed, bad LBA\n");
			decrementIndent();
		}

		return -1;
	}
	if ((count < 0) || (count > vcb->freeBlocks))
	{
		if(DEBUG)
		{
			printIndent();
			printf("markBlock: Failed, can't mark %d blocks as used, only %d are free\n", count, vcb->freeBlocks);
			decrementIndent();
		}

		return -1;
	}


	int wasBlocked = 0; //flag for if a bit set to 1 is hit
	int unfulfilled = count; //save requested amount so it can be modified
	int offset = LBA % INT_BIT_COUNT; //needed for starting at correct block

	//iterate through freeMap array
	for(int i = LBA / INT_BIT_COUNT; i < mapSize; i++)
	{
		if((unfulfilled == 0) || wasBlocked) break;
		
		//iterate through bits
		for(int j = offset; j < INT_BIT_COUNT; j++)
		{
			//check requested amount filled
			if(unfulfilled == 0) break;

			//check not out of bounds of volume
			if((i * INT_BIT_COUNT + j) >= vcb->totalBlocks)
			{
				if(DEBUG_DETAIL)
				{
					printIndent();
					printf("markBlock: end of volume/freeMap reached\n");
				}
				wasBlocked = 1;
				break;
			}

			//check bit is free
			if((freeMap[i] & (1 << j)) != 0)
			{
				if(DEBUG_DETAIL)
				{
					printIndent();
					printf("markBlock: non-free block found\n");
				}
				wasBlocked = 1;
				break;
			}

			//mark block as used
			freeMap[i] |= (1 << j);
			unfulfilled--;
		}
		

		offset = 0; //reset offset after first bitMap
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("markBlock: updating VCB and Map on volume\n");
	}

	//save updated info to disk
	int amount = count - unfulfilled;
	vcb->freeBlocks -= amount;
	vcb->usedBlocks += amount;
	commitMap();
	commitVCB();

	if(DEBUG)
	{
		printIndent();
		printf("markBlock: Finished marking %d blocks\n", amount);
		decrementIndent();
	}

	//return amount that was actually available
	return amount;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//This function marks a range of blocks as free in the file system's free block bitmap.
//this function shall not have a return value
void freeBlock(int LBA, int count)
{
	//check parameters
	if((LBA < 0) || (LBA >= vcb->totalBlocks)) return;
	if(count < 0) return;

	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("freeBlock: Request to free (%d blocks) starting from (LBA %d)\n", count, LBA);
	}

	//Initialize for the number of blocks requested and bit offset of first requested block
	int unfulfilled = count;
	int offset = LBA % INT_BIT_COUNT;
	int wasBlocked = 0; //flag for if end of map reached

	//checks each bit in the block's bitmap and set it to 0
	for(int i = LBA / INT_BIT_COUNT; i < mapSize; i++)
	{
		if((unfulfilled == 0) || wasBlocked) break;

		for(int j = offset; j < INT_BIT_COUNT; j++)
		{
			//check requested amount filled
			if(unfulfilled == 0) break;

			//check not out of bounds of volume
			if((i * INT_BIT_COUNT + j) >= vcb->totalBlocks)
			{
				wasBlocked = 1;
				break;
			}

			// clear the bit to 0 (unused)
			freeMap[i] &= ~(1 << j);
			unfulfilled--;
		}

		//reset the bit offset to 0 after processing the first block
		offset = 0;
	}

	//TODO: fix counters so double free() isn't counted

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("freeBlock: updating VCB and Map on volume\n");
	}

	//save updated info to disk
	vcb->freeBlocks += count;
	vcb->usedBlocks -= count;
	commitMap();
	commitVCB();

	if(DEBUG)
	{
		printIndent();
		printf("freeBlock: Free finished, %d blocks were released\n", count);
		decrementIndent();
	}
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//takes a request for blocks and returns an array of extents with the according blocks marked as used
//returns: an array of extent pointers where the last element = NULL
//returns: NULL when there are not enough blocks to allocate
//caller must handle memory allocated to return value by calling memFree_extentArray() after use
extent ** blockAlloc(int amount)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("blockAlloc: Trying to allocate %d blocks\n", amount);
	}
	
	//check available space
	if(amount > vcb->freeBlocks) 
	{
		if(DEBUG)
		{
			printIndent();
			printf("blockAlloc: Failed, only %d blocks are free\n", vcb->freeBlocks);
			decrementIndent();
		}

		return NULL;
	}

	//Allocate memory for the array of allocated blocks
	int extentCount = 0;
	int arraySize = 5; //is small bc not many extents will be needed
	extent ** allocatedBlocks = (extent**)calloc(arraySize, sizeof(extent *));
	if(allocatedBlocks == NULL)
	{
		if(DEBUG)
		{
			printIndent();
			printf("blockAlloc: Failed, couldn't malloc() return value\n");
			decrementIndent();
		}

		perror("\n*** failed to malloc \"allocatedBlocks\" in blockAlloc() ***\nerror");
		return NULL; //rather have this one get resolved instead of program force exit
		//exit(137); //especially bc resolutions will probably involve some free() calls
	}


	//iterate through bits in freeMap
	int LBA = -1;
	while(amount > 0)
	{
		//get LBA to first free block
		LBA++;
		int blockStatus = (freeMap[LBA / INT_BIT_COUNT] >> (LBA % INT_BIT_COUNT)) & 1;
		if(blockStatus == 1) continue;

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("blockAlloc: Free block found at LBA %d, attempting to mark %d blocks\n", LBA, amount);
		}

		//get number of blocks that can be contiguously allocated
		int allocated = markBlock(LBA, amount);
		if(allocated < 1) break;
		amount -= allocated;

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("blockAlloc: %d blocks marked, creating extent\n", allocated);
		}

		//create new extent for allocated blocks
		extent * ext = (extent*) malloc(sizeof(extent));
		if(ext == NULL)
		{
			if(DEBUG)
			{
				printIndent();
				printf("blockAlloc: Failed, couldn't malloc() extent\n");
				decrementIndent();
			}

			perror("\n*** failed to malloc \"ext\" in blockAlloc() ***\nerror");
			blockFree_extentArray(allocatedBlocks);
			memFree_extentArray(allocatedBlocks);
			return NULL;
		}
		ext->block = LBA;
		ext->count = allocated;

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("blockAlloc: Appending new extent to the return value\n");
		}

		//add to array but leave room for null terminator
		if(extentCount < arraySize - 1)
		{
			allocatedBlocks[extentCount] = ext;
			extentCount++;
		}
		//add to array and resize
		else
		{
			allocatedBlocks[extentCount] = ext;
			extentCount++;

			arraySize *= 2;
			allocatedBlocks = (extent **) realloc(allocatedBlocks, arraySize * sizeof(extent *));
			if(allocatedBlocks == NULL)
			{
				if(DEBUG)
				{
					printIndent();
					printf("blockAlloc: Failed, couldn't realloc() return value\n");
					decrementIndent();
				}

				//realloc() failing corrupts our volume by making it think empty blocks are used
				perror("\n*** failed to realloc \"allocatedBlocks\" in blockAlloc() ***\nerror");
				//blockFree_extentArray(allocatedBlocks);
				//memFree_extentArray(allocatedBlocks);
				exit(137);
			}
		}

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("blockAlloc: %d blocks still unallocated\n", amount);
			if(amount > 0) printf("\n");
		}
	}

	if(amount == 0)
	{
		if(DEBUG)
		{
			printIndent();
			printf("blockAlloc: Finished allocating blocks\n");
			decrementIndent();
		}

		//null terminate array and return
		allocatedBlocks[extentCount] = NULL;
		return allocatedBlocks;
	}
	else
	{
		if(DEBUG)
		{
			printIndent();
			printf("blockAlloc: CRITICAL FAILURE - despite \"existing\" freeSpace, there was a failure to allocate %d blocks\n", amount);
			decrementIndent();
		}

		blockFree_extentArray(allocatedBlocks);
		memFree_extentArray(allocatedBlocks);
		return NULL;
	}
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//frees the blocks held by the extent array
void blockFree_extentArray(extent ** array)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("blockFree_extentArray: Starting to free extents\n");
	}

	if(array != NULL)
	{
		for(int i = 0; array[i] != NULL; i++)
		{
			freeBlock(array[i]->block, array[i]->count);
		}
	}

	if(DEBUG)
	{
		printIndent();
		printf("blockFree_extentArray: Finished freeing extents\n");
		decrementIndent();
	}
}


//frees the memory used by the extent array
void memFree_extentArray(extent ** array)
{
	if(array != NULL)
	{
		for(int i = 0; array[i] != NULL; i++)
		{
			free(array[i]);
		}
		free(array);
	}
}
