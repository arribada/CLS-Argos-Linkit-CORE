#pragma once

#include <stdint.h>
#include "heap.h"

namespace MEMMANG
{
    uint32_t max_stack_usage();
    HeapStats_t heap_stats();
}
