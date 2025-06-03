#include "entry.h"

#include <string.h>

#include "vcb.h"

#define BASE_SIZE 50
#define DEFAULT_PERMISSIONS 0x777

#include "printIndent.h"

static int isDir(int entryLBA);


#define DEBUG 0
#define DEBUG_DETAIL 0
//creates a new file from the given parameters
//destLBA = an LBA to an existing entry ("existing" = block marked as used by an existing directory)
//returns: 0 for success, -1 for fail
int newFile(int destLBA, const char * name, const char * author)
{
	if(DEBUG) 
	{
		incrementIndent();
		printIndent();
		printf("newFile: Creating new file -> [%s] at LBA %d\n", name, destLBA);
	}
	
	//check parameters
	if((destLBA < vcb->rootLBA) || (destLBA >= vcb->totalBlocks)) 
	{
		if(DEBUG) 
		{
			printIndent();
			printf("newFile: Failed, bad LBA\n");
			decrementIndent();
		}

		return -1;
	}

	//create file entry from parameters
	entry ent;
	memset(&ent, 0, sizeof(ent));
	strcpy(ent.name, name);
	strcpy(ent.author, author);
	ent.type = FILE_ENTRY;
	ent.size = 0;
	ent.blockCount = 0;
	ent.created = time(NULL);
	ent.modified = time(NULL);
	ent.accessed = time(NULL);
	ent.permission = DEFAULT_PERMISSIONS;

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("newFile: Writing new entry to volume\n");
	}

	//commit entry to volume
	volumeWrite(&ent, sizeof(entry), destLBA);

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("newFile: Finished writing\n");
		// printIndent();
		// printf("\tnewFile: Entry's details\n");
		// printEntry(destLBA);
	}

	if(DEBUG)
	{       
		printIndent();
		printf("newFile: Finished creating file\n");
		decrementIndent();
	}

	return 0;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//creates a new directory from the given parameters
//destLBA = an LBA to an existing entry ("existing" = block marked as used by an existing directory)
//parentLBA = the LBA of the directory holding that entry
//returns: 0 for success, -1 for fail
int newDirectory(int destLBA, int parentLBA, const char * name, const char * author)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("newDirectory: Creating new directory -> [%s] at LBA %d w/ parent %d\n", name, destLBA, parentLBA);
	}

	//check parameters
	if((destLBA < vcb->rootLBA) || (destLBA >= vcb->totalBlocks))
	{
		if(DEBUG) 
		{
			printIndent();
			printf("newDirectory: Failed, bad LBA\n");
			decrementIndent();
		}

		return -1;
	}
	if(isDir(parentLBA) != 0)
	{
		if(DEBUG) 
		{
			printIndent();
			printf("newDirectory: Failed, parentLBA does not point to a DIR\n");
			decrementIndent();
		}

		return -1;
	}

	//create directory entry from parameters
	entry ent;
	memset(&ent, 0, sizeof(ent));
	strcpy(ent.name, name);
	strcpy(ent.author, author);
	ent.type = DIR_ENTRY;
	ent.size = sizeof(entry) * BASE_SIZE;
	ent.created = time(NULL);
	ent.modified = time(NULL);
	ent.accessed = time(NULL);
	ent.permission = DEFAULT_PERMISSIONS;

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("newDirectory: Writing new entry to volume\n");
	}

	//commit entry to volume
	volumeWrite(&ent, sizeof(entry), destLBA);

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("newDirectory: Finished writing\n");
		// printIndent();
		// printf("\tnewDirectory: Entry's details\n");
		// printEntry(destLBA);
	}

	if(DEBUG)
	{
		printIndent();
		printf("newDirectory: Growing DIR to base size\n");
	}

	//allocate space for directory's entries
	if(growEntry(destLBA, BASE_SIZE) != 0)
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("newDirectory: Reset entry to free and re-write volume\n");
		}

		//failed to grow directory so reset it
		ent.type = FREE_ENTRY;
		volumeWrite(&ent, sizeof(entry), destLBA);

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("newDirectory: Wrote free entry to volume\n");
		}

		if(DEBUG) 
		{
			printIndent();
			printf("newDirectory: Failed to grow DIR\n");
			decrementIndent();
		}

		return -1;
	}
	

	if(DEBUG)
	{
		printIndent();
		printf("newDirectory: Creating \"dot\" directory entries\n");
	}

	//create the "." directory entry
	volumeRead(&ent, sizeof(entry), destLBA);
	memset(ent.name, 0, MAX_NAME_LEN);
	strcpy(ent.name, ".");
	volumeWrite(&ent, sizeof(entry), getEntryBlock(destLBA, 0));

	//create the ".." directory entry
	volumeRead(&ent, sizeof(entry), parentLBA);

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("newDirectory: (btw this DIR's parent is [%s])\n", ent.name);
	}

	memset(ent.name, 0, MAX_NAME_LEN);
	strcpy(ent.name, "..");
	volumeWrite(&ent, sizeof(entry), getEntryBlock(destLBA, 1));


	if(DEBUG) 
	{
		printIndent();
		printf("newDirectory: Filling DIR with empty entries\n");
	}

	//fill allocated space with empty entries (needed because junk potentially leftover in block)
	memset(&ent, 0, sizeof(entry));
	for(int i = 2; i < BASE_SIZE; i++)
	{
		int LBA = getEntryBlock(destLBA, i);
		volumeWrite(&ent, sizeof(entry), LBA);
	}

	if(DEBUG) 
	{
		printIndent();
		printf("newDirectory: Completed directory creation\n");
		decrementIndent();
	}

	return 0;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
//searches through directory's entries to return the LBA of the first free entry
//returns: the LBA or -1 if not found
int getFreeEntry(int dirLBA)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("getFreeEntry: Getting free entry from DIR at LBA %d\n", dirLBA);
	}
	
	//check parameters
	if(isDir(dirLBA) != 0) 
	{
		if(DEBUG)
		{
			printIndent();
			printf("getFreeEntry: Failed, entry not DIR\n");
			decrementIndent();
		}

		return -1;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("getFreeEntry: Reading directory from volume\n");
	}

	entry ent;
	volumeRead(&ent, sizeof(entry), dirLBA);

	if(DEBUG)
	{
		printIndent();
		printf("getFreeEntry: Searching through DIR [%s] for free entry\n", ent.name);
	}
	
	//iterate through directory with getEntryBlock()
	int entryLBA = -1;
	for(int i = 0;; i++)
	{
		//get LBA to a directory entry
		entryLBA = getEntryBlock(dirLBA, i);
		if(entryLBA == -1) break; //end of directory reached

		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("\tgetFreeEntry: Reading entry in block %d from LBA %d\n", i, entryLBA);
		}

		//read entry from disk
		volumeRead(&ent, sizeof(entry), entryLBA);
		if(ent.type == FREE_ENTRY) break; //free entry found
	}

	//directory is full
	if(entryLBA == -1)
	{
		if(DEBUG)
		{
			printIndent();
			printf("getFreeEntry: DIR is full, attempting growth\n");
		}

		//increase DIR space
		if(growEntry(dirLBA, BASE_SIZE) == 0)
		{
			if(DEBUG)
			{
				printIndent();
				printf("getFreeEntry: Succesfully grew DIR, getting free entry\n");
			}

			entryLBA = getFreeEntry(dirLBA);
		}
		//growth failed
		else
		{
			if(DEBUG)
			{
				printIndent();
				printf("getFreeEntry: DIR growth failed\n");
			}

			entryLBA = -1;
		}
	}

	if(DEBUG)
	{
		printIndent();
		printf("getFreeEntry: Free entry found at LBA %d\n", entryLBA);
		decrementIndent();
	}

	return entryLBA;
}


#undef DEBUG
#undef DEBUG_DETAIL
#define DEBUG 0
#define DEBUG_DETAIL 0
static int isDir(int entryLBA)
{
	if(DEBUG_DETAIL)
	{
		incrementIndent();
		printIndent();
		printf("isDir: Checking LBA %d\n", entryLBA);
	}

	//check params
	if((entryLBA < 0) || (entryLBA >= vcb->totalBlocks)) 
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("isDir: Failed, bad LBA\n");
			decrementIndent();
		}

		return -1;
	}

	//check entry's type
	entry ent;
	volumeRead(&ent, sizeof(entry), entryLBA);
	if(ent.type != DIR_ENTRY)
	{
		if(DEBUG_DETAIL)
		{
			printIndent();
			printf("isDir: Not a DIR\n");
			decrementIndent();
		}
		
		return -1;
	}

	if(DEBUG_DETAIL)
	{
		printIndent();
		printf("isDir: Is a DIR\n");
		decrementIndent();
	}

	return 0;
}