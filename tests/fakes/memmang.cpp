#include "memmang.hpp"

uint32_t MEMMANG::max_stack_usage()
{
    return 0;
}

HeapStats_t MEMMANG::heap_stats()
{
    HeapStats_t stats{0, 0, 0, 0, 0, 0, 0};
    return stats;
}
