#include "entry.h"

#include "vcb.h"
#include "freeMap.h"

#include <string.h>
#include <stdlib.h>

#include "printIndent.h"

static int isEntry(int entryLBA);
static extent getEntryExtent(int entryLBA, int extOffset);
static int getExtentCount(int entryLBA);
static int overwriteExtent(int entryLBA, int extOffset, int block, int count);


#define DEBUG 0
#define DEBUG_DETAIL 0
//returns 0 if LBA points to entry, -1 if not
static int isEntry(int entryLBA)
{
	if(DEBUG_DETAIL)
	{
		incrementIndent();
		printIndent();
		printf("isEntry: Checking LBA %d\n", entryLBA);
	}

	//check params
	if((entryLBA < vcb->rootLBA) || (entryLBA >= vcb->totalBlocks)) 
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("isEntry: Failed, bad LBA\n");
			decrementIndent();
		}

		return -1;
	}

	//ensure entry is valid
	entry ent;
	volumeRead(&ent, sizeof(entry), entryLBA);
	if((ent.type != DIR_ENTRY) && (ent.type != FILE_ENTRY))
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("isEntry: LBA %d is not an entry\n", entryLBA);
			decrementIndent();
		}
		
		return -1;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("isEntry: LBA %d is an entry\n", entryLBA);
		decrementIndent();
	}

	return 0;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
/****************************************************
* Helper function freeEntry to free the blocks 
* associated with the entry located at the given entryLBA.
* The function reads the entry from disk, frees all 
* blocks assigned to the file, and updates the entry
* with a cleared name and FREE_ENTRY type.
****************************************************/
void freeEntry(int entryLBA)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("freeEntry: Freeing entry at LBA %d\n", entryLBA);
	}

	if(isEntry(entryLBA) != 0)
	{
		if(DEBUG)
		{
			printIndent();
			printf("freeEntry: failed, bad LBA\n");
			decrementIndent();
		}

		return;
	}

	if(DEBUG)
	{
		printIndent();
		printf("freeEntry: Calling freeBlock() on every block\n");
	}

	// Free all blocks assigned to file
	for(int i=0;; i++){
		int blockLBA = getEntryBlock(entryLBA, i);
		if (blockLBA == -1) break;
		freeBlock(blockLBA, 1);
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("freeEntry: Reading entry from volume\n");
	}

	// Read entry from disk
	entry selectedEntry;
	volumeRead(&selectedEntry,sizeof(entry), entryLBA);

	// Check if tertiary table exists
	int tertiaryLBA = selectedEntry.data[TABLE_SIZE - 1].block;
	if(tertiaryLBA != 0)
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("freeEntry: Reading tertiary table from volume\n");
		}

		// Read tertiary table from disk to get secondary table LBAs
		int tertiaryTable[INT_PER_BLOCK];		
		volumeRead(tertiaryTable, sizeof(tertiaryTable), tertiaryLBA);

		if(DEBUG)
		{
			printIndent();
			printf("freeEntry: Freeing secondary tables\n");
		}

		// Check LBAs in table and free them using freeBlock
		for (int i = 0; i < INT_PER_BLOCK; i++)
		{
			// If a secondary table exists, free it
			if(tertiaryTable[i] != 0) freeBlock(tertiaryTable[i], 1);
			else break;
		}

		if(DEBUG)
		{
			printIndent();
			printf("freeEntry: Freeing tertiary table\n");
		}

		// Free tertiary table block
		freeBlock(tertiaryLBA, 1);
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("freeEntry: Overwriting entry with blank\n");
	}

	// Update entry back to disk
	memset(&selectedEntry, 0, sizeof(entry));
	volumeWrite(&selectedEntry, sizeof(entry), entryLBA);

	if(DEBUG)
	{
		printIndent();
		printf("freeEntry: Finished freeing entry\n");
		decrementIndent();
	}
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//takes an existing entry and allocates blocks to append to the end of its extent table
//returns 0 for success, -1 for fail
int growEntry(int entryLBA, int count)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("growEntry: Starting to grow entry at LBA %d by %d blocks\n", entryLBA, count);
	}

	//check parameters
	if(isEntry(entryLBA) != 0) 
	{
		if(DEBUG)
		{
			printIndent();
			printf("growEntry: Failed, not an entry\n");
			decrementIndent();
		}

		return -1;
	}
	if((count < 0) || (count > vcb->freeBlocks)) 
	{
		if(DEBUG)
		{
			printIndent();
			printf("growEntry: Failed, can't allocate %d blocks only %d are free\n", count, vcb->freeBlocks);
			decrementIndent();
		}

		return -1;
	}


	if(DEBUG)
	{
		printIndent();
		printf("growEntry: Requesting blocks to be allocated\n");
	}

	//allocate the requested space
	extent ** allocBlocks = blockAlloc(count);
	if(allocBlocks == NULL)
	{
		if(DEBUG)
		{
			printIndent();
			printf("growEntry: Failed, blockAlloc() returned NULL\n");
			decrementIndent();
		}

		return -1;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("growEntry: Extents created by blockAlloc()\n");
	}

	//count the number of extents returned by blockAlloc()
	int allocCount;
	for(allocCount = 0; allocBlocks[allocCount] != NULL; allocCount++)
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("\tallocated[%d] -> (%d, %d)\n", 
			allocCount, allocBlocks[allocCount]->block, allocBlocks[allocCount]->count);
		}
	}

	if(DEBUG)
	{
		printIndent();
		printf("growEntry: Recieved %d new extents, getting current extent count\n", allocCount);
	}

	//get entry's extent count
	int extIndex = getExtentCount(entryLBA);

	//calculate if more secondary tables are needed
	int extsInSecondaries = extIndex - (TABLE_SIZE - 1); //skip past primary table
	int currTableCount = (extsInSecondaries + (EXT_PER_BLOCK - 1)) / EXT_PER_BLOCK;
	int remainingSpace = (currTableCount * EXT_PER_BLOCK) - extsInSecondaries;
	int notFitting = allocCount - remainingSpace;
	int newTableCount = (extsInSecondaries + allocCount + (EXT_PER_BLOCK - 1)) / EXT_PER_BLOCK;
	int newTables = newTableCount - currTableCount;

	if(DEBUG)
	{
		printIndent();
		printf("growEntry: Entry has %d extents w/ %d tables and needs %d more tables for %d extents\n", 
		extIndex, currTableCount, newTables, notFitting);
	}

	//add more secondary tables
	if(newTables > 0)
	{
		//check for tertiary table overflow
		if((newTables + currTableCount) >= INT_PER_BLOCK)
		{
			blockFree_extentArray(allocBlocks);
			memFree_extentArray(allocBlocks);

			if(DEBUG)
			{
				printIndent();
				printf("growEntry: Failed, %d more tables means entry would have %d tables when max is %ld\n",
				newTables, newTableCount, INT_PER_BLOCK);
				decrementIndent();
			}

			return -1;
		}

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("growEntry: Reading entry from volume\n");
		}

		//get entry from volume
		entry ent;
		volumeRead(&ent, sizeof(entry), entryLBA);

		//check tertiary table
		int tertiaryLBA = ent.data[TABLE_SIZE - 1].block;
		//create tertiary table if it DNE
		if(tertiaryLBA == 0)
		{
			if(DEBUG)
			{
				printIndent();
				printf("growEntry: Creating tertiary table bc it DNE\n");
			}

			//allocate block for table
			extent ** block = blockAlloc(1);
			if(block == NULL)
			{
				blockFree_extentArray(allocBlocks);
				memFree_extentArray(allocBlocks);

				if(DEBUG)
				{
					printIndent();
					printf("growEntry: Failed to allocate block for tertiary table\n");
					decrementIndent();
				}

				return -1;
			}

			if(DEBUG_DETAIL)
			{
				printIndent();
				printf("growEntry: Writing entry back to volume\n");
			}

			//update extent for tertiary table
			ent.data[TABLE_SIZE - 1] = *(block[0]);
			volumeWrite(&ent, sizeof(entry), entryLBA);
			memFree_extentArray(block);
			tertiaryLBA = ent.data[TABLE_SIZE - 1].block;
		}

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("growEntry: Reading tertiary table from LBA %d\n", tertiaryLBA);
		}
		
		//load tertiary
		int tertiaryTable[INT_PER_BLOCK];
		volumeRead(tertiaryTable, vcb->blockSize, tertiaryLBA);

		if(DEBUG)
		{
			printIndent();
			printf("growEntry: Allocating %d more table blocks for %d new extents\n", newTables, allocCount);
		}

		//allocate secondary tables
		extent ** newBlocks = blockAlloc(newTables);
		if(newBlocks == NULL)
		{
			blockFree_extentArray(allocBlocks);
			memFree_extentArray(allocBlocks);

			if(DEBUG)
			{
				printIndent();
				printf("growEntry: Failed to allocate blocks for secondary tables\n");
				decrementIndent();
			}

			return -1;
		}

		//write new LBAs to table
		int index = 0;
		int offset = 0;
		currTableCount = 0;
		for(int i = 0; i < INT_PER_BLOCK; i++)
		{
			//skip used table indexes
			if(tertiaryTable[i] != 0) 
			{
				currTableCount++;
				continue;
			}

			//end of allocated blocks
			if(newBlocks[index] == NULL) break;

			//this is so blocks are taken individually from the extents
			if(offset == newBlocks[index]->count)
			{
				offset = 0;
			}

			tertiaryTable[i] = offset + newBlocks[index]->block;
			offset++;
			index++;

			if(DEBUG_DETAIL)
			{
				printIndent();
				printf("growEntry: Wrote LBA %d to index %d in tertiary table\n", tertiaryTable[i], i);
			}
		}

		if(DEBUG)
		{
			printIndent();
			printf("growEntry: COMPARISON - og tertiary table actually had %d secondaries\n", currTableCount);
		}

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("growEntry: Writing tertiary table back to volume\n");
		}

		//write tertiary table back to volume
		volumeWrite(tertiaryTable, vcb->blockSize, tertiaryLBA);
	}


	if(DEBUG)
	{
		printIndent();
		printf("growEntry: Overwriting extent tables\n");
	}

	//write new extents to entry's tables
	int index = 0;
	for(int i = 0; i < allocCount; i++)
	{
		overwriteExtent(entryLBA, extIndex, allocBlocks[i]->block, allocBlocks[i]->count);
		extIndex++;
	}

	if(DEBUG)
	{
		printIndent();
		printf("growEntry: Updating entry's blockCount\n");
	}

	//update entry info on disk
	entry ent;
	volumeRead(&ent, sizeof(entry), entryLBA);
	ent.blockCount += count;
	volumeWrite(&ent, sizeof(entry), entryLBA);

	if(DEBUG)
	{
		printIndent();
		printf("growEntry: Finished growing entry\n");
		decrementIndent();
	}

	//return success
	return 0;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//takes an existing entry and frees blocks from the end of its extent table
//it will modify entry.blockCount but not entry.size
//returns 0 for success, -1 for fail
int shrinkEntry(int entryLBA, int count)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("shrinkEntry: Starting to shrink entry @ LBA %d by %d blocks\n", entryLBA, count);
	}

	//check parameters
	if(isEntry(entryLBA) != 0)
	{
		if(DEBUG)
		{
			printIndent();
			printf("shrinkEntry: Failed, bad LBA\n");
			decrementIndent();
		}
		return -1;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("shrinkEntry: Reading entry from volume\n");
	}

	//cap count to entry.blockCount
	entry ent;
	volumeRead(&ent, sizeof(ent), entryLBA);
	int tertiaryLBA = ent.data[TABLE_SIZE - 1].block;
	if(count > ent.blockCount)
	{
		if(DEBUG)
		{
			printIndent();
			printf("shrinkEntry: Capped count to entry's block count of %d\n", ent.blockCount);
		}

		count = ent.blockCount;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("shrinkEntry: Getting entry's extent count\n");
	}

	//get last extent of extent table
	int extCount = getExtentCount(entryLBA);
	int newExtCount = extCount;

	if(DEBUG)
	{
		printIndent();
		printf("shrinkEntry: Entry has %d extents\n", extCount);
	}
	if(DEBUG_DETAIL) incrementIndent();

	//iterate through entry's extents backwards and overwrite
	for(int i = extCount - 1; ; i--)
	{
		//requested number of blocks has been free'd
		if(count <= 0)
		{
			newExtCount = i + 1;
			break;
		}

		//get entry's extent
		extent ext = getEntryExtent(entryLBA, i);
		if((ext.block == -1) && (ext.count == -1))
		{
			if(DEBUG)
			{
				printIndent();
				printf("shrinkEntry: Failed, -1 returned by getEntryExtent()\n");
				decrementIndent();
			}
			if(DEBUG_DETAIL) decrementIndent();

			return -1;
		}

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("shrinkEntry: Extent %d -> (%d, %d)\n", i, ext.block, ext.count);
		}

		//amount to reduce from extent
		int amount;
		if(ext.count > count) amount = count;
		else amount = ext.count;
		count -= amount;

		//LBA for freeBlock()
		int LBA = ext.block + ext.count - amount;

		//update extent
		if(ext.block == LBA)
		{
			ext.block = 0;
			ext.count = 0;
		}
		else ext.count -= amount;
		
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("shrinkEntry: Freeing (%d, %d)\n", LBA, amount);
		}

		//update freeMap & entry
		freeBlock(LBA, amount);

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("shrinkEntry: Overwriting extent w/ (%d, %d)\n", ext.block, ext.count);
		}

		overwriteExtent(entryLBA, i, ext.block, ext.count);
	}

	if(DEBUG_DETAIL) decrementIndent();
	if(DEBUG)
	{
		printIndent();
		printf("shrinkEntry: Modified %d extents\n", extCount - newExtCount);
	}

	//calculate if less secondary tables are needed
	extCount -= (TABLE_SIZE - 1); //skip past primary table
	newExtCount -= (TABLE_SIZE - 1);
	int oldTableCount = (extCount + (EXT_PER_BLOCK - 1)) / EXT_PER_BLOCK;
	int newTableCount = (newExtCount + (EXT_PER_BLOCK - 1)) / EXT_PER_BLOCK;

	//free unused secondary tables
	if(oldTableCount != newTableCount)
	{
		if(DEBUG)
		{
			printIndent();
			printf("shrinkEntry: Erasing %d secondary extent tables\n", oldTableCount - newTableCount);
		}

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("shrinkEntry: Reading tertiary table from LBA %d\n", tertiaryLBA);
		}
		
		//load tertiary
		int tertiaryTable[INT_PER_BLOCK];
		volumeRead(tertiaryTable, vcb->blockSize, tertiaryLBA);

		for(int i = oldTableCount - 1; i >= newTableCount; i--)
		{
			tertiaryTable[i] = 0;
		}

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("shrinkEntry: Writing tertiary table back to volume\n");
		}

		//write tertiary table back to volume
		volumeWrite(tertiaryTable, vcb->blockSize, tertiaryLBA);

		//free tertiary table
		if(newTableCount == 0)
		{
			if(DEBUG_DETAIL)
			{
				printIndent();
				printf("growEntry: Freeing tertiary table\n");
			}

			freeBlock(tertiaryLBA, 1);

			if(DEBUG_DETAIL)
			{
				printIndent();
				printf("growEntry: Resetting entry's extent for tertiary table\n");
			}

			overwriteExtent(entryLBA, TABLE_SIZE - 1, 0, 0);
		}
	}

	if(DEBUG)
	{
		printIndent();
		printf("shrinkEntry: Updating entry's blockCount\n");
	}

	//volumeRead() entry again because it's been modified
	volumeRead(&ent, sizeof(entry), entryLBA);
	ent.blockCount -= count;
	volumeWrite(&ent, sizeof(entry), entryLBA);

	if(DEBUG)
	{
		printIndent();
		printf("shrinkEntry: Finished shrinking entry\n");
		decrementIndent();
	}
	
	return 0;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//searches through entry's extent tables to return the LBA of the block at offset
//returns LBA of block or -1 if not found
int getEntryBlock(int entryLBA, int blockOffset)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("getEntryBlock: Getting (block %d) of entry @ (LBA %d)\n", blockOffset, entryLBA);
	}

	//check parameters
	if(isEntry(entryLBA) != 0)
	{
		if(DEBUG)
		{
			printIndent();
			printf("getEntryBlock: Failed, bad LBA\n");
			decrementIndent();
		}

		return -1;
	}
	if (blockOffset < 0)
	{
		if(DEBUG)
		{
			printIndent();
			printf("getEntryBlock: Failed, bad offset\n");
			decrementIndent();
		}

		return -1;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("getEntryBlock: Reading entry from volume\n");
	}

	//read entry from volume
	entry ent;
	volumeRead(&ent, sizeof(entry), entryLBA);

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("getEntryBlock: Iterating through primary table\n");
	}

	int retVal = 0;
	//iterate through extents in primary table
	for(int i = 0; i < TABLE_SIZE-1; i++)
	{
		//get & check extent
		extent ext = ent.data[i];
		if((ext.block == 0) && (ext.count == 0))
		{
			//end of allocated blocks
			retVal = -1;
			break;
		}

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("\tgetEntryBlock: offset = %d, extent[%d] -> (%d, %d)\n", blockOffset, i, ext.block, ext.count);
		}

		//return LBA to block at offset
		if(blockOffset < ext.count)
		{
			retVal = ext.block + blockOffset;
			break;
		}
		//or offset not found so get next extent
		else blockOffset -= ext.count;
	}

	//return block found from primary table
	if(retVal != 0)
	{
		if(DEBUG)
		{
			printIndent();
			printf("getEntryBlock: Actual LBA of block is %d\n", retVal);
			decrementIndent();
		}

		return retVal;
	}


	//check tertiary table exists
	int tertiaryLBA = ent.data[TABLE_SIZE-1].block;
	if(tertiaryLBA == 0)
	{
		if(DEBUG)
		{
			printIndent();
			printf("getEntryBlock: Failed, entry does not have tertiary table\n");
			decrementIndent();
		}

		return -1;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("getEntryBlock: Reading tertiary table from LBA %d\n", tertiaryLBA);
	}

	//read tertiary table from volume
	int tertiaryTable[INT_PER_BLOCK];
	volumeRead(&tertiaryTable, vcb->blockSize, tertiaryLBA);

	//iterate through tertiary table for pointers to seconday tables
	for(int i = 0; i < INT_PER_BLOCK; i++)
	{
		if(retVal != 0) break;

		//get & check LBA to secondary table
		int secondaryLBA = tertiaryTable[i];
		if(secondaryLBA == 0) retVal = -1; //end of tertiary table reached

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("getEntryBlock: Reading secondary table %d from LBA %d\n", i, secondaryLBA);
		}

		//read secondary table from volume
		extent secondaryTable[EXT_PER_BLOCK];
		volumeRead(&secondaryTable, vcb->blockSize, secondaryLBA);

		//iterate through extents in secondary table
		for(int j = 0; j < EXT_PER_BLOCK; j++)
		{
			//get & check extent
			extent ext = secondaryTable[j];
			if((ext.block == 0) && (ext.count == 0))
			{
				//end of allocated blocks
				retVal = -1;
				break;
			}

			if(DEBUG_DETAIL)
			{
				printIndent();
				printf("\tgetEntryBlock: offset = %d, extent[%d] -> (%d, %d)\n", blockOffset, j, ext.block, ext.count);
			}

			//return LBA to block at offset
			if(blockOffset < ext.count) 
			{
				retVal = ext.block + blockOffset;
				break;
			}
			//or offset not found so get next extent
			else blockOffset -= ext.count;
		}
	}

	if(DEBUG)
	{
		printIndent();
		printf("getEntryBlock: Actual LBA of block is %d\n", retVal);
		decrementIndent();
	}

	return retVal;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//searches through entry's extent tables and returns the extent at the offset
//returns the extent if it exists, or an extent of (-1, -1) if it DNE
static extent getEntryExtent(int entryLBA, int extOffset)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("getEntryExtent: Getting extent %d of entry at LBA %d\n", extOffset, entryLBA);
	}

	extent retVal = {-1, -1};

	//check params
	if(isEntry(entryLBA) != 0) 
	{
		if(DEBUG)
		{
			printIndent();
			printf("getEntryExtent: Failed, not an entry\n");
			decrementIndent();
		}

		return retVal;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("getEntryExtent: Reading entry from volume\n");
	}

	//get entry
	entry ent;
	volumeRead(&ent, sizeof(entry), entryLBA);

	//return extent from primary table
	if(extOffset < TABLE_SIZE - 1)
	{
		retVal = ent.data[extOffset];

		if(DEBUG)
		{
			printIndent();
			printf("getEntryExtent: Extent found in primary table -> (%d, %d)\n", retVal.block, retVal.count);
			decrementIndent();
		}

		return retVal;
	}
	//setup offset for secondary table
	else extOffset -= (TABLE_SIZE - 1);


	//check tertiary table
	int tertiaryLBA = ent.data[TABLE_SIZE - 1].block;
	if(tertiaryLBA == 0)
	{
		if(DEBUG)
		{
			printIndent();
			printf("getEntryExtent: Failed, entry doesn't have tertiary table\n");
			decrementIndent();
		}

		return retVal;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("getEntryExtent: Reading tertiary table (LBA %d) from volume\n", tertiaryLBA);
	}

	//get tertiary table
	int tertiaryTable[INT_PER_BLOCK];
	volumeRead(tertiaryTable, sizeof(int) * INT_PER_BLOCK, tertiaryLBA);

	//calculate extent's position
	int extIndex = extOffset % EXT_PER_BLOCK;
	int tableIndex = extOffset / EXT_PER_BLOCK;
	int tableLBA = tertiaryTable[tableIndex];


	//check secondary table
	if(tableLBA == 0)
	{
		if(DEBUG)
		{
			int count;
			for(count = 0; count < INT_PER_BLOCK; count++)
			{
				if(tertiaryTable[count] == 0) break;
			}

			printIndent();
			printf("getEntryExtent: Failed, wanted secondary table %d but entry only has %d tables\n", tableIndex, count);
			decrementIndent();
		}

		return retVal;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("getEntryExtent: Reading secondary table (LBA %d) from volume\n", tableLBA);
	}

	//get extent from secondary table
	extent secondaryTable[EXT_PER_BLOCK];
	volumeRead(secondaryTable, sizeof(extent) * EXT_PER_BLOCK, tableLBA);
	retVal = secondaryTable[extIndex];

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("getEntryExtent: Extent found at index %d of secondary table %d\n", extIndex, tableIndex);
	}

	if(DEBUG)
	{
		printIndent();
		printf("getEntryExtent: Extent found in secondary table -> (%d, %d)\n", retVal.block, retVal.count);
		decrementIndent();
	}

	return retVal;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//returns the number of extents held by an entry's tables
//returns -1 if LBA points to not an entry
//works by searching through the entry's extent tables for the first extent with value of (0, 0)
static int getExtentCount(int entryLBA)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("getExtentCount: Getting extent count of entry at LBA %d\n", entryLBA);
	}

	//check parameter
	if(isEntry(entryLBA) != 0)
	{
		if(DEBUG)
		{
			printIndent();
			printf("getExtentCount: Failed, not an entry\n");
			decrementIndent();
		}

		return -1;
	}


	//can maybe get turned into O(log n) by doing a binary search
	//using the entry's block count and BASE_SIZE to calculate some step amount
	//currently it's just O(n) as you can see


	//iterate through entry's extents
	int offset;
	for(offset = 0; ; offset++)
	{
		//get extent from entry
		extent ext = getEntryExtent(entryLBA, offset);

		// if(DEBUG_DETAIL)
		// {
		// 	printIndent();
		// 	printf("getExtentCount: Offset %d -> (%d, %d)\n", offset, ext.block, ext.count);
		// }

		//should mean end of secondary block reached
		if((ext.block == -1) && (ext.count == -1)) break;

		//checks if extent is free
		if((ext.block == 0) && (ext.count == 0)) break;
	}

	if(DEBUG)
	{
		printIndent();
		printf("getExtentCount: Entry has %d extents\n", offset);
		decrementIndent();
	}

	return offset;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//overwrites an already existing extent in the entry with the passed in data
//return 0 for success or -1 for failure
static int overwriteExtent(int entryLBA, int extOffset, int block, int count)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("overwriteExtent: Attempting to overwrite extent %d of entry at LBA %d with (%d, %d)\n", 
		extOffset, entryLBA, block, count);
	}

	//check params
	if(isEntry(entryLBA) != 0) 
	{
		if(DEBUG)
		{
			printIndent();
			printf("overwriteExtent: Failed, not an entry\n");
			decrementIndent();
		}

		return -1;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("overwriteExtent: Reading entry from volume\n");
	}

	//get entry
	entry ent;
	volumeRead(&ent, sizeof(entry), entryLBA);

	//overwrite extent in primary table
	if(extOffset < TABLE_SIZE - 1)
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("overwriteExtent: Extent found at index %d of primary table\n", extOffset);
			printIndent();
			printf("overwriteExtent: Prevous extent value -> (%d, %d)\n", 
			ent.data[extOffset].block, ent.data[extOffset].count);
		}

		extent ext;
		ext.block = block;
		ext.count = count;
		ent.data[extOffset] = ext;

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("overwriteExtent: Writing entry back to volume\n");
		}

		volumeWrite(&ent, sizeof(entry), entryLBA);

		if(DEBUG)
		{
			printIndent();
			printf("overwriteExtent: Overwrote extent in primary table\n");
			decrementIndent();
		}

		return 0;
	}
	//setup offset for secondary table
	else extOffset -= (TABLE_SIZE - 1);


	//check tertiary table
	int tertiaryLBA = ent.data[TABLE_SIZE - 1].block;
	if(tertiaryLBA == 0)
	{
		if(DEBUG)
		{
			printIndent();
			printf("overwriteExtent: Failed, entry doesn't have tertiary table\n");
			decrementIndent();
		}

		return -1;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("overwriteExtent: Reading tertiary table (LBA %d) from volume\n", tertiaryLBA);
	}

	//get tertiary table
	int tertiaryTable[INT_PER_BLOCK];
	volumeRead(tertiaryTable, sizeof(int) * INT_PER_BLOCK, tertiaryLBA);

	//calculate extent's position
	int extIndex = extOffset % EXT_PER_BLOCK;
	int tableIndex = extOffset / EXT_PER_BLOCK;
	int tableLBA = tertiaryTable[tableIndex];


	//check secondary table
	if(tableLBA == 0)
	{
		if(DEBUG)
		{
			int count;
			for(count = 0; count < INT_PER_BLOCK; count++)
			{
				if(tertiaryTable[count] == 0) break;
			}

			printIndent();
			printf("overwriteExtent: Failed, wanted secondary table %d but entry only has %d tables\n", tableIndex, count);
			decrementIndent();
		}

		return -1;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("overwriteExtent: Reading secondary table (LBA %d) from volume\n", tableLBA);
	}

	//overwrite extent in secondary table
	extent secondaryTable[EXT_PER_BLOCK];
	volumeRead(secondaryTable, sizeof(extent) * EXT_PER_BLOCK, tableLBA);

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("overwriteExtent: Extent found at index %d of secondary table %d\n", extIndex, tableIndex);
		printIndent();
		printf("overwriteExtent: Prevous extent value -> (%d, %d)\n", 
		secondaryTable[extIndex].block, secondaryTable[extIndex].count);
	}

	extent ext;
	ext.block = block;
	ext.count = count;
	secondaryTable[extIndex] = ext;

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("overwriteExtent: Writing secondary table back to volume\n");
	}

	volumeWrite(secondaryTable, sizeof(extent) * EXT_PER_BLOCK, tableLBA);

	if(DEBUG)
	{
		printIndent();
		printf("overwriteExtent: Overwrote extent in secondary table\n");
		decrementIndent();
	}

	return 0;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//prints the entry at the specified LBA
void printEntry(int LBA)
{
	incrementIndent();

	if(isEntry(LBA) != 0)
	{
		printf("LBA %d is not an entry", LBA);
		decrementIndent();
		return;
	}

	entry ent;
	volumeRead(&ent, sizeof(entry), LBA);

	printIndent();
	printf("name: %s\n", ent.name);

	printIndent();
	printf("type: ");
	if(ent.type == FILE_ENTRY) printf("file\n");
	else if (ent.type == DIR_ENTRY) printf("directory\n");
	else printf("%d\n", ent.type);

	printIndent();
	printf("size: %ld\n", ent.size);
	printIndent();
	printf("blockCount: %d\n", ent.blockCount);

	//print extents
	for(int i = 0;; i++)
	{
		extent ext = getEntryExtent(LBA, i);
		if((ext.block == 0) && (ext.count == 0)) break;
		if((ext.block == -1) && (ext.count == -1)) break;

		if((i % 5) == 0) printIndent();
		else printf("\t");

		printf("(%d, %d)", ext.block, ext.count);

		if(((i+1) % 5) == 0) printf("\n");
	}

	decrementIndent();
}
