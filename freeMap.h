#ifndef FREEMAP_H
#define FREEMAP_H

#include "entry.h"

void initMap(int totalBlocks);
void freeFreeMap();

void volumeReadMap();
void commitMap();

int markBlock(int LBA, int count);
void freeBlock(int LBA, int count);

extent ** blockAlloc(int blocks);
void blockFree_extentArray(extent ** array);
void memFree_extentArray(extent ** array);

#endif
