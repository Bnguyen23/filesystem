// Compile Commands
// gcc -c -o entry.o entry.c
// gcc -c -o freeMap.o freeMap.c
// gcc -c -o vcb.o vcb.c
// gcc -c -o main.o main.c
// gcc -o main main.o fsLow.o vcb.o freeMap.o entry.o -lm
// ./main
// ./Hexdump/hexdump.linux test.file --count 8 --start 0

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "fsLow.h"
#include "vcb.h"
#include "entry.h"
#include "mfs.h"
#include "parsePath.h"
#include "printIndent.h"


//#include <errno.h>
int main()
{
	// errno = EBADR;
	// perror("\n*** failed to malloc() \"iobuffer\" in openVolume() ***\nerror");
	// return 0;

	char * fileName = "test.file";
	uint64_t volSize = 9994240;
	uint64_t blockSize = 512;
	startPartitionSystem(fileName, &volSize, &blockSize);

	decrementIndent();

	//open volume
	printf("\n\nmain: Opening Volume\n\n");
	if(openVolume(9994240, 512) != 0)
	{
		printf("main: Error opening volume\n");
		return 1;
	}
	else printf("\nmain: Volume opened\n\n");


	int created = fs_mkdir("/apple/banana", 12345);
	printf("\nmain: created -> %d\n\n", created);
	int parsed = parsePath("/apple/banana");
	printf("\nmain: parsed -> %d\n\n\n", parsed);


	// printEntry(parsed);
	// printf("\n\n");
	// shrinkEntry(parsed, 5);
	// printf("\n\n");
	// printEntry(parsed);
	// printf("\n\n");
	// growEntry(parsed, 23);
	// printf("\n\n");
	// printEntry(parsed);
	// printf("\n\n");
	// shrinkEntry(parsed, 5);
	// printf("\n\n");
	// printEntry(parsed);


	incrementIndent();
	for(int i = 0; i < 50; i++)
	{
		printf("main: executing grow #%d\n", i+1);
		growEntry(parsed, 23);
	}
	decrementIndent();

	printf("\n\n");
	printEntry(parsed);
	printf("\n\n");
	shrinkEntry(parsed, 981981250);
	printf("\n\n");
	printEntry(parsed);


	//close volume
	printf("\n\nmain: Closing Volume\n\n");
	if(closeVolume() != 0)
	{
		printf("main: Error closing volume\n\n\n");
		return 1;
	}
	else printf("main: Volume closed\n\n\n");

	closePartitionSystem();

	return 0;
}
