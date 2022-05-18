#include "heap.h"

// Binds newlib memory allocators to that of freeRTOS heap manager

void * operator new( size_t size )
{
    return pvPortMalloc( size );
}

void * operator new[]( size_t size )
{
    return pvPortMalloc( size );
}

void operator delete( void * ptr )
{
    vPortFree( ptr );
}

void operator delete[]( void * ptr )
{
    vPortFree( ptr );
}

void operator delete(void* ptr, unsigned int sz)
{
    (void) sz;
    vPortFree( ptr );
}

void operator delete[](void* ptr, unsigned int sz)
{
    (void) sz;
    vPortFree( ptr );
}

