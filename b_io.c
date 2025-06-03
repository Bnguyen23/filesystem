/**************************************************************
* Class:  CSC-415-0# Fall 2021
* Names: 
* Student IDs:
* GitHub Name:
* Group Name:
* Project: Basic File System
*
* File: b_io.c
*
* Description: Basic File System - Key File I/O Operations
*
**************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>			// for malloc
#include <string.h>			// for memcpy
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "b_io.h"
#include "parsePath.h"
#include "mfs.h"
#include "vcb.h"
#include "entry.h"

#define MAXFCBS 20
#define B_CHUNK_SIZE 512

typedef struct b_fcb
	{
	/** TODO add al the information you need in the file control block **/
	char * buf;		//holds the open file buffer
	int index;		//holds the current position in the buffer
	int buflen;		//holds how many valid bytes are in the buffer
	int access;     //determines read/write access to current file
	int filePos;    //holds position in current file
	int entryLBA;   //holds current open file's LBA
	} b_fcb;
	
b_fcb fcbArray[MAXFCBS];

int startup = 0;	//Indicates that this has not been initialized

//Method to initialize our file system
void b_init ()
	{
	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].buf = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free FCB element
b_io_fd b_getFCB ()//call getFreeEntry
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].buf == NULL)
			{
			return i;		//Not thread safe (But do not worry about it for this assignment)
			}
		}
	return (-1);  //all in use
	}
	
// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags for open
// O_RDONLY, O_WRONLY, or O_RDWR
b_io_fd b_open (char * filename, int flags)
	{		
		if (startup == 0) b_init();  //Initialize our system

		int retValue = parsePath(filename);
		if(retValue == -1){
			printf("Filename %s does not exist\n", filename);
		}

		b_io_fd fd = b_getFCB();
		if(fd == -1){
			printf("No more free control blocks\n");
			return -1;
		}

		//allocate space for 10 blocks of memory
		fcbArray[fd].buf = malloc(sizeof(B_CHUNK_SIZE * 10));
		if(fcbArray[fd].buf == NULL){
			perror("could not malloc fcbArray buffer\n");
			exit(137);
		}

		//determines if flags sets access to read only
		if((flags & O_RDONLY == O_RDONLY)){
			 fcbArray[fd].access |= O_RDONLY;
		}

		//determines if flags sets access to write only
		if((flags & O_WRONLY == O_WRONLY)){
			fcbArray[fd].access |= O_WRONLY;
		}
		//determines if flags set access read & write
		if(flags & O_RDWR== O_RDWR){
			fcbArray[fd].access |= O_RDWR;
		}
		//checks access of file to verify remaining flags
		if(fcbArray[fd].access == O_WRONLY || fcbArray[fd].access == O_RDWR){
			if((flags & O_CREAT) == O_CREAT){
				if(retValue == -1){
					//if file doesn't exist, create file
					fs_mkfile(filename,fcbArray[fd].access);
					int index = parsePath(filename);
					int LBA = getEntryBlock(index, 0);
					//read entry in from disk
					entry ent;
					volumeRead(&ent, B_CHUNK_SIZE, LBA);
					//set creation/modifying/accessing time
					ent.created = time(NULL);
					ent.modified = time(NULL);
					ent.accessed = time(NULL);
					volumeWrite(&ent, B_CHUNK_SIZE, LBA);
					}
				else{
					("Filename: %s already exists\n", filename);
					return -1;
				}
			}
			if(flags & O_TRUNC == O_TRUNC){
				if(retValue == -1){
					printf("Cannot truncate file that does not exist\n");
					return -1;
				}
				int LBA = getEntryBlock(fcbArray[fd].entryLBA, 0);
				entry ent;
				volumeRead(&ent, B_CHUNK_SIZE, LBA);
				//set all bytes of entry to 0
				memset(&ent, 0, sizeof(ent));
				//set modifying/accessing time
				ent.size = 0;				
				ent.modified = time(NULL);
				ent.accessed = time(NULL);
				//set fcb values of file to 0
				fcbArray[fd].index = 0;
				fcbArray[fd].filePos = 0;
				fcbArray[fd].buflen = 0;
				//write entry back to disk
				volumeWrite(&ent, B_CHUNK_SIZE, LBA);
			}
			if(flags & O_APPEND == O_APPEND){
				int LBA = getEntryBlock(fcbArray[fd].entryLBA, 0);
				entry ent;
				volumeRead(&ent, B_CHUNK_SIZE, LBA);
				fcbArray[fd].filePos = ent.size - 1;
				//set modifying/accessing time
				ent.modified = time(NULL);
				ent.accessed = time(NULL);
				//write entry back to disk
				volumeWrite(&ent, B_CHUNK_SIZE, LBA);
			}
		}
	
	return (fd);						// all set
	}


// Interface to seek function	
int b_seek(b_io_fd fd, off_t offset, int whence) {

    if (startup == 0) b_init();  // Initialize our system

    // Check that fd is between 0 and (MAXFCBS-1)
    if ((fd < 0) || (fd >= MAXFCBS)) {
		perror("b_seek: invalid file descriptor");
        return (-1); // Invalid file descriptor
    }

	entry ent;
	volumeRead(&ent, sizeof(entry), fcbArray[fd].entryLBA);
	int size = ent.size;

    switch (whence) {
        // Beginning of file
        case SEEK_SET:
            fcbArray[fd].filePos = offset;
            break;
        // Current position
        case SEEK_CUR:
            fcbArray[fd].filePos += offset;
            break;
        // End of file
        case SEEK_END:
			// buf.st_size is from fs_stat
            fcbArray[fd].filePos = size + offset;
            break;
        default:
            return (-1); // Invalid whence
    }

    // Prevent seeking past the end when in read-only mode
    if ((fcbArray[fd].access & O_RDONLY) == O_RDONLY && fcbArray[fd].filePos > size) {
        return (-1); // Invalid file position
    }

	//ensure the new file position is within the valid range
	if(fcbArray[fd].filePos <0){
		fcbArray[fd].filePos = 0;
	}

    // Return the new offset from the beginning of the file
    return (fcbArray[fd].filePos);
}




// Interface to write function	
int b_write (b_io_fd fd, char * buffer, int count)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		perror("b_write: invalid file descriptor");
		return (-1); 					//invalid file descriptor
		}
		//check for open extents


	int bytesWritten = 0;
	int remainingBytes = count;	//number of bytes remaining to be read

	// tracks called buffer's current position
	int userBufferPos = 0;
	int blockPos = fcbArray[fd].filePos / vcb->blockSize;
	int offsetInBlock = fcbArray[fd].filePos % vcb->blockSize;
	int startingLBA = getEntryBlock(fcbArray[fd].entryLBA, blockPos);
	//if the LBA is -1 then we are at the end of file
	if(startingLBA == -1){
		printf("eof\n");
		return 0;
	}

	while(remainingBytes > 0){

		// calculate the number of bytes to copy from block buffer
		int bytesToCopy = vcb->blockSize - offsetInBlock;

		// if the number of bytes to copy is greather than the reamining bytes
		if(bytesToCopy > remainingBytes){
			bytesToCopy = remainingBytes;
		}
		char systemBuffer[vcb->blockSize];
		memcpy(systemBuffer, buffer + bytesWritten, bytesToCopy);
		volumeWrite(systemBuffer + offsetInBlock, vcb->blockSize, startingLBA);


		bytesWritten+= bytesToCopy;
		remainingBytes -= bytesToCopy;
		fcbArray[fd].filePos += bytesToCopy;
		blockPos++;
		offsetInBlock = 0;
		
	}
		
		
	return (bytesWritten); //Change this
	}



// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill 
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
int b_read (b_io_fd fd, char * buffer, int count)
	{

	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		perror("b_read: invalid file descriptor");
		return (-1); 					//invalid file descriptor
		}
		

	int bytesRead = 0; 	// number of bytes read from the file
	int remainingBytes = count;	//number of bytes remaining to be read

	// cap remainingBytes to the difference between the file's size and the current file position
    int fileSize = fcbArray[fd].filePos;
    if (fcbArray[fd].filePos + remainingBytes > fileSize) {
        remainingBytes = fileSize - fcbArray[fd].filePos;
    }
	
	// calculate the block position based on current file position
	int blockPos = fcbArray[fd].filePos / vcb->blockSize;
	// caculate the offset within the block based on current file position
	int offsetInBlock = fcbArray[fd].filePos % vcb->blockSize;

	//continue reading while there are no more bytes to read
	while(remainingBytes > 0){
		// get current LBA
		int currentLBA = getEntryBlock(fcbArray[fd].entryLBA, blockPos);
		//if the LBA is -1 then we are at the end of file
		if(currentLBA == -1){
			break;
		}

		//read the current block into block buffer
		char blockBuffer[vcb->blockSize];
		volumeRead(blockBuffer, vcb->blockSize, currentLBA);

		// calculate the number of bytes to copy from block buffer
		int bytesToCopy = vcb->blockSize - offsetInBlock;

		// if the number of bytes to copy is greather than the reamining bytes
		if(bytesToCopy > remainingBytes){
			bytesToCopy = remainingBytes;
		}

		//copy the data from the block buffer to the user's buffer
		memcpy(buffer + bytesRead, blockBuffer + offsetInBlock, bytesToCopy);

		// update the number of bytes read, remaining, and file position
		bytesRead += bytesToCopy;
		remainingBytes -= bytesToCopy;
		fcbArray[fd].filePos += bytesToCopy;
		
		// move to the next block
		blockPos ++;
		
		//reset the offset within the block for next iteration
		offsetInBlock = 0;
	}
	return (bytesRead);	// return total number of bytes read
	}
	
// Interface to Close the file	
int b_close (b_io_fd fd){

	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		perror("b_close: invalid file descriptor");
		return (-1); //invalid file descriptor
		}
		
	//get file control block for file descriptor
	b_fcb fcb = fcbArray[fd];

	//calulate the current number of blocks allocated to file
	entry ent;
	volumeRead(&ent, sizeof(entry), fcb.entryLBA);
	int blockCount = ent.blockCount;
	int allocatedBlocks = blockCount;

	//calculate the number of blocks required for the current file
	int requiredBlocks = (fcb.filePos + vcb->blockSize-1)/ vcb->blockSize;

	//if the extra blocsk are found, shrink the entry to free the extra blocks
	if (requiredBlocks > 0){
		// use shrinkEntry to release the extra blocks
		int result = shrinkEntry(fcb.entryLBA, requiredBlocks);

		if(result < 0){
			perror("b_close: failed to shrink entry");
			return result;
		}
		
		// update the file control block
		ent.blockCount -= requiredBlocks;
	}

	// reset the file descriptor in the file control blockarray
	fcbArray[fd].entryLBA = -1;

	return 0; 

}
