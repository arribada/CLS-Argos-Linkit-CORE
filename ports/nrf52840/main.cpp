#include "scheduler.hpp"
#include "filesystem.hpp"
#include "gentracker.hpp"

#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)


// Global contexts
Scheduler  *main_sched;
FileSystem *main_filesystem;

// FSM initial state -> BootState
FSM_INITIAL_STATE(GenTracker, BootState)

using fsm_handle = GenTracker;

void main() {
	// Setup global contexts
	main_filesystem = new RamFileSystem(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
	main_sched = new Scheduler();

	// This will initialise the FSM
	fsm_handle::start();

	// The scheduler should run forever.  Any run-time exceptions should be handled by the
	// lambda function which passes them into the FSM
	main_sched->run([](int err) {
		ErrorEvent e;
		e.error_code = err;
		fsm_handle::dispatch(e);
	});
}
