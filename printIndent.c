#include "printIndent.h"
#include <stdio.h>

static int indentLevel = 0;

void printIndent()
{
    for(int i = 0; i < indentLevel; i++) printf("\t");
}

void incrementIndent()
{
    indentLevel++;
}

void decrementIndent()
{
    indentLevel--;
}
