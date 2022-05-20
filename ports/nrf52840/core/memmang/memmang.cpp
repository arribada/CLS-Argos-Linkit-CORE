#include "app_util.h"
#include "memmang.hpp"
#include "heap.h"
#include "debug.hpp"

static constexpr uint32_t stack_fill_word = 0xCAFED00D;

static const uint32_t stack_top = reinterpret_cast<uint32_t>(&__StackTop);
static const uint32_t stack_limit = reinterpret_cast<uint32_t>(&__StackLimit);

uint32_t MEMMANG::max_stack_usage()
{
    // Stack monitoring relies on gcc_startup_nrf52840.S having prefilled the stack region with the stack_fill_word
    // Reading stack_top directly causes a hardfault so offset by 256 bytes
    // NOTE: This function scans the entire free stack region so it slow
	for (uint32_t i = stack_limit; i < stack_top - 256; i+=sizeof(stack_fill_word))
	{
		uint32_t val = *reinterpret_cast<uint32_t*>(i);
		if (val != stack_fill_word)
        {
            return stack_top - i;
        }
	}

	return 0;
}

HeapStats_t MEMMANG::heap_stats()
{
    HeapStats_t stats;
    vPortGetHeapStats(&stats);
    return stats;
}
