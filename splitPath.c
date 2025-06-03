#include "splitPath.h"

#include <stdio.h>
#include <string.h>
#include <stdio.h>

#include "printIndent.h"
#define DEBUG 0

void splitPathname(const char * pathname, char * path, char * name)
{
	if(DEBUG) 
	{
		incrementIndent();
		printIndent();
		printf("splitPath: pathname -> [%s]\n", pathname);
	}

	char * finalSeparator = strrchr(pathname, '/'); //pointer to last directory separator

	//split pathname along separator
	if(finalSeparator != NULL)
	{
		int pathLength = finalSeparator - pathname;
		strcpy(name, pathname + pathLength + 1); //copy last path node, +1 is so separator is not in name
		strncpy(path, pathname, pathLength); //copy path while excluding last path node
		path[pathLength] = '\0'; //null-terminate because partial string doesn't have one
		if(strlen(path) == 0) strcpy(path, "/"); //ensures the '/' for base path remains
	}
	//separator doesn't exist
	else
	{
		strcpy(path, "\0");
		strcpy(name, pathname);
	}

	if(DEBUG)
	{
		printIndent();
		printf("splitPath: path = [%s], len=%ld\n", path, strlen(path));
		printIndent();
		printf("splitPath: name = [%s], len=%ld\n", name, strlen(name));
		decrementIndent();
	}
}