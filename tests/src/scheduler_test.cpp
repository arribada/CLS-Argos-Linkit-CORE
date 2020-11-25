#include "linux_timer.hpp"
#include "scheduler.hpp"
#include <iostream>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

using namespace std;

static Timer *timer = new LinuxTimer;
static Scheduler *scheduler = new Scheduler(timer);


TEST_GROUP(Scheduler)
{
	void setup() {
		MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
		timer->start();
	}

	void teardown() {
		MemoryLeakWarningPlugin::restoreNewDeleteOverloads();
		timer->stop();
	}
};


TEST(Scheduler, SchedulerSingleTaskSingleShot)
{
	static bool fired = false;
	scheduler->post_task_prio([]() { fired = true; }, scheduler->DEFAULT_PRIORITY, 0);
	scheduler->run([](int e){}, 10);
	CHECK_TRUE(fired);
}

TEST(Scheduler, SchedulerDeferredSingleTaskSingleShot)
{
	static bool fired = false;
	scheduler->post_task_prio([]() { fired = true; }, scheduler->DEFAULT_PRIORITY, 5);
	scheduler->run([](int e){}, 100);
	CHECK_TRUE(fired);
}

TEST(Scheduler, SchedulerDeferredMultiTaskSingleShot)
{
	static bool fired[2] = { false };
	scheduler->post_task_prio([]() { fired[0] = true; }, scheduler->DEFAULT_PRIORITY, 5);
	scheduler->post_task_prio([]() { fired[1] = true; }, scheduler->DEFAULT_PRIORITY, 10);
	scheduler->run([](int e){}, 100);
	CHECK_TRUE(fired[0]);
	CHECK_TRUE(fired[1]);
}

TEST(Scheduler, SchedulerCancelDeferredTaskSingleShot)
{
	static bool fired = false;
	Task t = []() { fired = true; };
	scheduler->post_task_prio(t, scheduler->DEFAULT_PRIORITY, 5);
	scheduler->run([](int e){}, 1);
	scheduler->cancel_task(t);
	CHECK_FALSE(scheduler->is_scheduled(t));
	scheduler->run([](int e){}, 100);
	CHECK_FALSE(fired);
}

TEST(Scheduler, SchedulerIsScheduledApiCall)
{
	static bool fired = false;
	Task t = []() { fired = true; };
	scheduler->post_task_prio(t, scheduler->DEFAULT_PRIORITY, 5);
	scheduler->run([](int e){}, 1);
	CHECK_TRUE(scheduler->is_scheduled(t));
	scheduler->run([](int e){}, 100);
	CHECK_TRUE(fired);
}

TEST(Scheduler, SchedulerPriorityOrdering)
{
	static int order = 0;
	static int fired[7] = { -1 };
	scheduler->post_task_prio([]() { fired[0] = order++; }, 7, 5);
	scheduler->post_task_prio([]() { fired[1] = order++; }, 6, 5);
	scheduler->post_task_prio([]() { fired[2] = order++; }, 5, 5);
	scheduler->post_task_prio([]() { fired[3] = order++; }, 4, 5);
	scheduler->post_task_prio([]() { fired[4] = order++; }, 3, 5);
	scheduler->post_task_prio([]() { fired[5] = order++; }, 2, 5);
	scheduler->post_task_prio([]() { fired[6] = order++; }, 1, 5);
	scheduler->run([](int e){}, 100);
	CHECK_EQUAL(6, fired[0]);
	CHECK_EQUAL(5, fired[1]);
	CHECK_EQUAL(4, fired[2]);
	CHECK_EQUAL(3, fired[3]);
	CHECK_EQUAL(2, fired[4]);
	CHECK_EQUAL(1, fired[5]);
	CHECK_EQUAL(0, fired[6]);
}
