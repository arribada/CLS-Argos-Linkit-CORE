#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "heap.h"

// Binds newlib memory allocators to that of freeRTOS heap manager

void* sbrk(int incr)
{
    // Newlib standard heap usage banned
    // Instead newlib should automatically use the overriden malloc etc... functions below
    (void) incr;

    configASSERT(false);

    errno = ENOMEM;
    return (char*)-1;
}

void* _sbrk(int incr)
{
    return sbrk(incr);
}

void* _sbrk_r(struct _reent* r, int incr)
{
    (void) r;
    return _sbrk(incr);
}

void free(void* ptr)
{
    return vPortFree(ptr);
}

void _free_r(struct _reent* r, void* ptr)
{
    (void) r;
    return free(ptr);
}

void* malloc(size_t size)
{
    return pvPortMalloc(size);
}

void* _malloc_r(struct _reent* r, size_t size)
{
    (void) r;
    return malloc(size);
}

void* realloc(void* ptr, size_t size)
{
    void* mem;
    mem = malloc(size);
    if (mem != NULL) {
        memcpy(mem, ptr, size);
        free(ptr);
    }
    return mem;
}

void* _realloc_r(struct _reent* r, void* ptr, size_t size)
{
    (void) r;
    return realloc(ptr, size);
}

void* calloc(size_t nitems, size_t size)
{
    void* mem;
    size_t bytes = nitems * size;

    mem = malloc(bytes);
    if (mem != NULL)
        memset(mem, 0, bytes);
    return mem;
}

void* _calloc_r(struct _reent* r, size_t nitems, size_t size)
{
    (void) r;
    return calloc(nitems, size);
}

void* memalign(size_t alignment, size_t size)
{
    configASSERT(false); // Not implemented

    return NULL;
}

void* _memalign_r(struct _reent* r, size_t alignment, size_t size)
{
    (void) r;
    return memalign(alignment, size);
}
