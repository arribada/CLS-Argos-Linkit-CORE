#include "service.hpp"
#include "memmang.hpp"

class MemoryMonitorService : public Service {
public:
	MemoryMonitorService() : Service(ServiceIdentifier::MEMORY_MONITOR, "MEMORY") {
	}

	void service_initiate() {
		HeapStats_t heap_stats = MEMMANG::heap_stats();
		DEBUG_INFO("MemoryMonitorService: HEAP: %d min, %d free, %d freeblk, %d allocs, %d frees",
			heap_stats.xMinimumEverFreeBytesRemaining,
			heap_stats.xAvailableHeapSpaceInBytes,
			heap_stats.xNumberOfFreeBlocks,
			heap_stats.xNumberOfSuccessfulAllocations,
			heap_stats.xNumberOfSuccessfulFrees);
		DEBUG_INFO("MemoryMonitorService: STACK: %u max", MEMMANG::max_stack_usage());
		service_complete();
	}

	unsigned int service_next_schedule_in_ms() override {
		return 60 * 1000;
		// Run every 12 hours
		return 12 * 3600 * 1000;
	}

	bool service_is_enabled() override { return true; }
	void service_init() override {}
	void service_term() override {}
};
