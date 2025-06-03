#include "mfs.h"

#include <string.h>
#include <stdlib.h>

#include "parsePath.h"
#include "splitPath.h"
#include "entry.h"
#include "vcb.h"
#include "freeMap.h"

#include "printIndent.h"
#define DEBUG 0

int checkDirectoryEmpty(int dirLBA);
int removeEntry(int parentLBA, const char *entryName);
void freeEntry(int entryLBA);


int checkDirectoryEmpty(int dirLBA);
int removeEntry(int parentLBA, const char *entryName);
void freeEntry(int entryLBA);

int fs_mkfile(const char *pathname, mode_t mode)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("fs_mkfile: Trying to make FILE -> [%s]\n", pathname);
	}

	//ensure pathname is free
	if(parsePath(pathname) != -1)
	{
		if(DEBUG)
		{
			printIndent();
			printf("fs_mkfile: Creation failed, pathname not free\n");
			decrementIndent();
		}
		return -1;
	}

	if(DEBUG)
	{
		printIndent();
		printf("fs_mkfile: Splitting name from path\n");
	}

	//extract path to parent directory + name of new directory
	char * path = malloc(strlen(pathname));
	if(path == NULL)
	{
		perror("*** failed to malloc \"path\" in fs_mkdir() ***\n");
		exit(137);
	}
	char * name = malloc(MAX_NAME_LEN);
	if(name == NULL)
	{
		perror("*** failed to malloc \"name\" in fs_mkdir() ***\n");
		exit(137);
	}
	splitPathname(pathname, path, name);

	if(DEBUG) 
	{
		printIndent();
		printf("fs_mkfile: parsePath() the parent [%s]\n", path);
	}

	//get LBA to parent directory from parsePath()
	int parentLBA = parsePath(path);

	//parent DNE, so file creation fails
	if(parentLBA == -1)
	{
		if(DEBUG)
		{
			printIndent();
			printf("fs_mkfile: Creation failed, parent directory DNE\n");
			decrementIndent();
		}

		return -1;
	}

	if(DEBUG)
	{
		printIndent();
		printf("fs_mkfile: Parent has LBA %d\n", parentLBA);
	}

	//ensure parent is a directory
	entry ent;
	volumeRead(&ent, sizeof(entry), parentLBA);
	if(ent.type != DIR_ENTRY)
	{
		if(DEBUG) 
		{
			printIndent();
			printf("fs_mkfile: Creation failed, parent is not DIR\n");
			decrementIndent();
		}
		return -1;
	}

	if(DEBUG)
	{
		printIndent();
		printf("fs_mkfile: Parent is DIR, getting free entry\n");
	}

	//create new directory at parent's first free entry
	int entryLBA = getFreeEntry(parentLBA);

	if(DEBUG)
	{
		printIndent();
		printf("fs_mkdir: Creating new FILE [%s] at LBA %d\n", name, entryLBA);
	}

	newFile(entryLBA, name, SYSTEM_NAME);

	if(DEBUG)
	{
		printIndent();
		printf("fs_mkfile: File Created\n");
		decrementIndent();
	}

	//cleanup and return success
	free(path);
	free(name);
	return 0;
}

int fs_mkdir(const char *pathname, mode_t mode)
{
	if(DEBUG)
	{
		incrementIndent();
		printIndent();
		printf("fs_mkdir: Trying to make DIR -> [%s]\n", pathname);
	}

	//ensure pathname is free
	if(parsePath(pathname) != -1)
	{
		if(DEBUG)
		{
			printIndent();
			printf("fs_mkdir: Failed because pathname is used\n");
			decrementIndent();
		}
		return -1;
	}

	if(DEBUG)
	{
		printIndent();
		printf("fs_mkdir: Splitting name from path\n");
	}

	//extract path to parent directory + name of new directory
	char * path = malloc(strlen(pathname));
	if(path == NULL)
	{
		perror("*** failed to malloc \"path\" in fs_mkdir() ***\nerror");
		exit(137);
	}
	char * name = malloc(MAX_NAME_LEN);
	if(name == NULL)
	{
		perror("*** failed to malloc \"name\" in fs_mkdir() ***\nerror");
		exit(137);
	}
	splitPathname(pathname, path, name);

	if(DEBUG) 
	{
		printIndent();
		printf("fs_mkdir: Attempting to parsePath() the parent [%s]\n", path);
	}

	//get LBA to parent directory from parsePath()
	int parentLBA = parsePath(path);

	//parent DNE, so create it
	if(parentLBA == -1)
	{
		if(DEBUG)
		{
			printIndent();
			printf("fs_mkdir: Creating parent DIR because it DNE\n");
		}

		//check creation success and get LBA
		if(fs_mkdir(path, 777) == 0)
		{
			if(DEBUG)
			{
				printIndent();
				printf("fs_mkdir: Parent DIR created\n");
			}

			parentLBA = parsePath(path);
		}
		else
		{
			if(DEBUG)
			{
				printIndent();
				printf("fs_mkdir: Failed to create parent DIR\n");
				decrementIndent();
			}

			perror("*** failed to create directory ***\n");
			return -1;
		}
	}


	if(DEBUG)
	{
		printIndent();
		printf("fs_mkdir: Parent has LBA %d\n", parentLBA);
	}

	//ensure parent is a directory
	entry ent;
	volumeRead(&ent, sizeof(entry), parentLBA);
	if(ent.type != DIR_ENTRY)
	{
		if(DEBUG)
		{
			printIndent();
			printf("fs_mkdir: Creating DIR failed, parent is not DIR\n");
			decrementIndent();
		}
		return -1;
	}

	if(DEBUG)
	{
		printIndent();
		printf("fs_mkdir: Parent is DIR, getting free entry\n");
	}

	//create new directory at parent's first free entry
	int entryLBA = getFreeEntry(parentLBA);

	if(DEBUG)
	{
		printIndent();
		printf("fs_mkdir: Creating new DIR [%s] at LBA %d\n", name, entryLBA);
	}
	
	newDirectory(entryLBA, parentLBA, name, SYSTEM_NAME);

	if(DEBUG)
	{
		printIndent();
		printf("fs_mkdir: Finished creation\n");
		decrementIndent();
	}

	//cleanup and return success
	free(path);
	free(name);
	return 0;
}


/****************************************************
*  fs_isFile function, checks if the specified path
*  is a file and returns true if it is, 
*  false otherwise.
****************************************************/
int fs_isFile(char *filename){
	// parse to get the LBA of the specified filename
	int fileLBA = parsePath(filename);
	if(fileLBA == -1) return 0; 

	// Read entry at the LBA
	entry fileEntry;
	volumeRead(&fileEntry, sizeof(entry), fileLBA);

	// Check if the entry type is FILE_ENTRY, 
	// return true if it is.
	return (fileEntry.type == FILE_ENTRY);
}

/****************************************************
*  fs_isDir function, does the same as isFile but for
*  a directory instead of a file.
****************************************************/
int fs_isDir(char *pathname){
	// parse to get the LBA of the specified pathname
	int dirLBA = parsePath(pathname);
	if(dirLBA == -1) return 0;

	// Read entry at the LBA
	entry dirEntry;
	volumeRead(&dirEntry, sizeof(entry), dirLBA);

	// Check if the entry type is DIR_ENTRY, 
	// return true if it is.
	return (dirEntry.type == DIR_ENTRY);
}

/****************************************************
*  fs_rmdir function checks if the specified path is 
*  a valid file or directory. If it is, and dir is
*  empty, delete dir and update parent DE list.
****************************************************/
int fs_rmdir(const char *pathname){
	// parsePath to get the LBA of the specified pathname
	int dirLBA = parsePath(pathname);
	if(dirLBA == -1) return -1;

	// Read the entry at the LBA
	entry ent;
	volumeRead(&ent, sizeof(entry), dirLBA);

	// Check if the entry type is DIR_ENTRY
	if(ent.type != DIR_ENTRY) return -1; // Not a directory

	// Check if the directory is empty
	if (!checkDirectoryEmpty(dirLBA)) return -1;

	// Free the directory entry and the allocated memory
	freeEntry(dirLBA);

	return 0;
}



/*******************************************************
* Helper function checkDirectoryEmpty checks if a
* directory is empty by iterating through its entries.
* Returns 0 if dir is empty, and -1 if not.
*******************************************************/
int checkDirectoryEmpty(int dirLBA){
	entry ent;
	// Loop indefinitely until a return statement is executed
	// start with i=2 to skip past the dot directories
	for (int i=2;; i++){
		// Get LBA of entry at current index i
		int entryLBA = getEntryBlock(dirLBA, i);

		// if entryLBA is -1 reached end of dir and no
		// non-empty entry was found.
		if(entryLBA == -1) return 0;

		// Read entry at current LBA
		volumeRead(&ent, sizeof(entry), entryLBA);

		// Ensure entry is free 
		if ((ent.type != FREE_ENTRY) || (strlen(ent.name) > 0)){
			return -1;
		}
	}
}
