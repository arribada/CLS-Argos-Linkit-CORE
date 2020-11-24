#include "timer.hpp"
#include "scheduler.hpp"
#include <iostream>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

using namespace std;


TEST_GROUP(Scheduler)
{
	void setup() {
		MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
		Timer::start();
	}

	void teardown() {
		MemoryLeakWarningPlugin::restoreNewDeleteOverloads();
		Timer::stop();
	}
};


TEST(Scheduler, SchedulerSingleTaskSingleShot)
{
	static bool fired = false;
	Scheduler::post_task_prio([]() { fired = true; }, Scheduler::DEFAULT_PRIORITY, 0);
	Scheduler::run([](int e){}, 10);
	CHECK_TRUE(fired);
}

TEST(Scheduler, SchedulerDeferredSingleTaskSingleShot)
{
	static bool fired = false;
	Scheduler::post_task_prio([]() { fired = true; }, Scheduler::DEFAULT_PRIORITY, 5);
	Scheduler::run([](int e){}, 100);
	CHECK_TRUE(fired);
}

TEST(Scheduler, SchedulerDeferredMultiTaskSingleShot)
{
	static bool fired[2] = { false };
	Scheduler::post_task_prio([]() { fired[0] = true; }, Scheduler::DEFAULT_PRIORITY, 5);
	Scheduler::post_task_prio([]() { fired[1] = true; }, Scheduler::DEFAULT_PRIORITY, 10);
	Scheduler::run([](int e){}, 100);
	CHECK_TRUE(fired[0]);
	CHECK_TRUE(fired[1]);
}

TEST(Scheduler, SchedulerCancelDeferredTaskSingleShot)
{
	static bool fired = false;
	Task t = []() { fired = true; };
	Scheduler::post_task_prio(t, Scheduler::DEFAULT_PRIORITY, 5);
	Scheduler::run([](int e){}, 1);
	Scheduler::cancel_task(t);
	CHECK_FALSE(Scheduler::is_scheduled(t));
	Scheduler::run([](int e){}, 100);
	CHECK_FALSE(fired);
}

TEST(Scheduler, SchedulerIsScheduledApiCall)
{
	static bool fired = false;
	Task t = []() { fired = true; };
	Scheduler::post_task_prio(t, Scheduler::DEFAULT_PRIORITY, 5);
	Scheduler::run([](int e){}, 1);
	CHECK_TRUE(Scheduler::is_scheduled(t));
	Scheduler::run([](int e){}, 100);
	CHECK_TRUE(fired);
}

TEST(Scheduler, SchedulerPriorityOrdering)
{
	static int order = 0;
	static int fired[7] = { -1 };
	Scheduler::post_task_prio([]() { fired[0] = order++; }, 7, 5);
	Scheduler::post_task_prio([]() { fired[1] = order++; }, 6, 5);
	Scheduler::post_task_prio([]() { fired[2] = order++; }, 5, 5);
	Scheduler::post_task_prio([]() { fired[3] = order++; }, 4, 5);
	Scheduler::post_task_prio([]() { fired[4] = order++; }, 3, 5);
	Scheduler::post_task_prio([]() { fired[5] = order++; }, 2, 5);
	Scheduler::post_task_prio([]() { fired[6] = order++; }, 1, 5);
	Scheduler::run([](int e){}, 100);
	CHECK_EQUAL(6, fired[0]);
	CHECK_EQUAL(5, fired[1]);
	CHECK_EQUAL(4, fired[2]);
	CHECK_EQUAL(3, fired[3]);
	CHECK_EQUAL(2, fired[4]);
	CHECK_EQUAL(1, fired[5]);
	CHECK_EQUAL(0, fired[6]);
}
