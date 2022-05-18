#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct xHeapStats
{
	size_t xAvailableHeapSpaceInBytes;
	size_t xSizeOfLargestFreeBlockInBytes;
	size_t xSizeOfSmallestFreeBlockInBytes;
	size_t xNumberOfFreeBlocks;
	size_t xMinimumEverFreeBytesRemaining;
	size_t xNumberOfSuccessfulAllocations;
	size_t xNumberOfSuccessfulFrees;
} HeapStats_t;

void vPortGetHeapStats( HeapStats_t *xHeapStats ) { (void) xHeapStats; }

#ifdef __cplusplus
}
#endif