#include "scheduler.hpp"
#include "filesystem.hpp"
#include "gentracker.hpp"
#include "linux_timer.hpp"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)

// Global contexts
FileSystem *main_filesystem;
Scheduler *main_scheduler;

// FSM initial state -> BootState
FSM_INITIAL_STATE(GenTracker, BootState)

using fsm_handle = GenTracker;

void main() {
	// Local timer
	Timer *timer = new LinuxTimer;

	// Setup global contexts
	main_filesystem = new RamFileSystem(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
	main_scheduler = new Scheduler(timer);

	// This will initialise the FSM
	fsm_handle::start();

	// The scheduler should run forever.  Any run-time exceptions should be handled and passed to FSM.
	try {
		while (true) {
			main_scheduler->run();
		}
	} catch (int e) {
		ErrorEvent event;
		event.error_code = e;
		fsm_handle::dispatch(event);
	}
}
