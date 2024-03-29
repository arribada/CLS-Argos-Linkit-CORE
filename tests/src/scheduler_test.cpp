#include "linux_timer.hpp"
#include "scheduler.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

using namespace std;

static Timer *timer = new LinuxTimer;
static Scheduler *scheduler = new Scheduler(timer);


TEST_GROUP(Scheduler)
{
	void setup() {
		timer->start();
	}

	void teardown() {
		timer->stop();
		scheduler->clear_all();
	}

	void delay_ms(uint64_t ms)
	{
		uint64_t start = timer->get_counter();
		while (timer->get_counter() - start < ms)
		{}
	}
};


TEST(Scheduler, SchedulerSingleTaskSingleShot)
{
	static bool fired = false;
	scheduler->post_task_prio([]() { fired = true; }, "SchedulerSingleTaskSingleShot", scheduler->DEFAULT_PRIORITY, 0);
	delay_ms(10);
	scheduler->run();
	CHECK_TRUE(fired);
}

TEST(Scheduler, SchedulerDeferredSingleTaskSingleShot)
{
	static bool fired = false;
	scheduler->post_task_prio([]() { fired = true; }, "SchedulerSingleTaskSingleShot", scheduler->DEFAULT_PRIORITY, 5);
	delay_ms(100);
	scheduler->run();
	CHECK_TRUE(fired);
}

TEST(Scheduler, SchedulerDeferredMultiTaskSingleShot)
{
	static bool fired[2] = { false };
	scheduler->post_task_prio([]() { fired[0] = true; }, "SchedulerDeferredMultiTaskSingleShot0", scheduler->DEFAULT_PRIORITY, 5);
	scheduler->post_task_prio([]() { fired[1] = true; }, "SchedulerDeferredMultiTaskSingleShot1", scheduler->DEFAULT_PRIORITY, 10);
	delay_ms(100);
	scheduler->run();
	CHECK_TRUE(fired[0]);
	CHECK_TRUE(fired[1]);
}

TEST(Scheduler, SchedulerCancelSingleTaskSingleShot)
{
	static bool fired = false;
	auto t = []() { fired = true; };
	auto task_handle = scheduler->post_task_prio(t, "SchedulerCancelSingleTaskSingleShot", scheduler->DEFAULT_PRIORITY);
	scheduler->cancel_task(task_handle);
	scheduler->run();
	CHECK_FALSE(scheduler->is_scheduled(task_handle));
	delay_ms(100);
	scheduler->run();
	CHECK_FALSE(fired);
}

TEST(Scheduler, SchedulerCancelDeferredTaskSingleShot)
{
	static bool fired = false;
	auto t = []() { fired = true; };
	auto task_handle = scheduler->post_task_prio(t, "SchedulerCancelDeferredTaskSingleShot", scheduler->DEFAULT_PRIORITY, 5);
	scheduler->run();
	scheduler->cancel_task(task_handle);
	CHECK_FALSE(scheduler->is_scheduled(task_handle));
	delay_ms(100);
	scheduler->run();
	CHECK_FALSE(fired);
}

TEST(Scheduler, SchedulerIsScheduledApiCall)
{
	static bool fired = false;
	auto t = []() { fired = true; };
	auto task_handle = scheduler->post_task_prio(t, "SchedulerIsScheduledApiCall", scheduler->DEFAULT_PRIORITY, 5);
	scheduler->run();
	CHECK_TRUE(scheduler->is_scheduled(task_handle));
	delay_ms(100);
	scheduler->run();
	CHECK_TRUE(fired);
}

TEST(Scheduler, SchedulerPriorityOrdering)
{
	static int order = 0;
	static int fired[7] = { -1 };
	scheduler->post_task_prio([]() { fired[0] = order++; }, "SchedulerPriorityOrdering7", 7, 5);
	scheduler->post_task_prio([]() { fired[1] = order++; }, "SchedulerPriorityOrdering6", 6, 5);
	scheduler->post_task_prio([]() { fired[2] = order++; }, "SchedulerPriorityOrdering5", 5, 5);
	scheduler->post_task_prio([]() { fired[3] = order++; }, "SchedulerPriorityOrdering4", 4, 5);
	scheduler->post_task_prio([]() { fired[4] = order++; }, "SchedulerPriorityOrdering3", 3, 5);
	scheduler->post_task_prio([]() { fired[5] = order++; }, "SchedulerPriorityOrdering2", 2, 5);
	scheduler->post_task_prio([]() { fired[6] = order++; }, "SchedulerPriorityOrdering1", 1, 5);
	delay_ms(100);
	scheduler->run();
	CHECK_EQUAL(6, fired[0]);
	CHECK_EQUAL(5, fired[1]);
	CHECK_EQUAL(4, fired[2]);
	CHECK_EQUAL(3, fired[3]);
	CHECK_EQUAL(2, fired[4]);
	CHECK_EQUAL(1, fired[5]);
	CHECK_EQUAL(0, fired[6]);
}

TEST(Scheduler, SchedulerSamePriorityOrdering)
{
    static int order = 0;
    static int fired[7] = { -1 };
    scheduler->post_task_prio([]() { printf("fired0\n"); fired[0] = order++; }, "SchedulerPriorityOrdering0");
    scheduler->post_task_prio([]() { printf("fired1\n"); fired[1] = order++; }, "SchedulerPriorityOrdering1");
    scheduler->post_task_prio([]() { printf("fired2\n"); fired[2] = order++; }, "SchedulerPriorityOrdering2");
    scheduler->post_task_prio([]() { printf("fired3\n"); fired[3] = order++; }, "SchedulerPriorityOrdering3");
    scheduler->post_task_prio([]() { printf("fired4\n"); fired[4] = order++; }, "SchedulerPriorityOrdering4");
    scheduler->post_task_prio([]() { printf("fired5\n"); fired[5] = order++; }, "SchedulerPriorityOrdering5");
    scheduler->post_task_prio([]() { printf("fired6\n"); fired[6] = order++; }, "SchedulerPriorityOrdering6");
    scheduler->run();
    CHECK_EQUAL(0, fired[0]);
    CHECK_EQUAL(1, fired[1]);
    CHECK_EQUAL(2, fired[2]);
    CHECK_EQUAL(3, fired[3]);
    CHECK_EQUAL(4, fired[4]);
    CHECK_EQUAL(5, fired[5]);
    CHECK_EQUAL(6, fired[6]);
}

TEST(Scheduler, SchedulerSamePriorityOrderingWithDelay)
{
    static int order = 0;
    static int fired[7] = { -1 };
    scheduler->post_task_prio([]() { printf("fired0\n"); fired[0] = order++; }, "SchedulerPriorityOrdering0", 0, 10);
    scheduler->post_task_prio([]() { printf("fired1\n"); fired[1] = order++; }, "SchedulerPriorityOrdering1", 0, 10);
    scheduler->post_task_prio([]() { printf("fired2\n"); fired[2] = order++; }, "SchedulerPriorityOrdering2", 0, 10);
    scheduler->post_task_prio([]() { printf("fired3\n"); fired[3] = order++; }, "SchedulerPriorityOrdering3", 0, 10);
    scheduler->post_task_prio([]() { printf("fired4\n"); fired[4] = order++; }, "SchedulerPriorityOrdering4", 0, 10);
    scheduler->post_task_prio([]() { printf("fired5\n"); fired[5] = order++; }, "SchedulerPriorityOrdering5", 0, 10);
    scheduler->post_task_prio([]() { printf("fired6\n"); fired[6] = order++; }, "SchedulerPriorityOrdering6", 0, 10);
    delay_ms(20);
    scheduler->run();
    CHECK_EQUAL(0, fired[0]);
    CHECK_EQUAL(1, fired[1]);
    CHECK_EQUAL(2, fired[2]);
    CHECK_EQUAL(3, fired[3]);
    CHECK_EQUAL(4, fired[4]);
    CHECK_EQUAL(5, fired[5]);
    CHECK_EQUAL(6, fired[6]);
}

TEST(Scheduler, SchedulerWithRecursion)
{
    static int order = 0;
    scheduler->post_task_prio([]() {
    	printf("counter0=%u\n", order); order++;
        scheduler->post_task_prio([]() {
        	printf("counter0=%u\n", order); order++;
        }, "SchedulerPriorityOrdering", 0, 10);
    }, "SchedulerPriorityOrdering", 0, 0);
    scheduler->post_task_prio([]() { printf("counter1=%u\n", order); order++; }, "SchedulerPriorityOrdering", 0, 0);
    scheduler->post_task_prio([]() { printf("counter2=%u\n", order); order++; }, "SchedulerPriorityOrdering", 0, 0);
    scheduler->post_task_prio([]() { printf("counter3=%u\n", order); order++; }, "SchedulerPriorityOrdering", 0, 0);
    scheduler->post_task_prio([]() { printf("counter4=%u\n", order); order++; }, "SchedulerPriorityOrdering", 0, 0);
    for (unsigned int i = 0; i < 100; i++) {
		delay_ms(1);
		scheduler->run();
    }
}

TEST(Scheduler, SchedulerMultithreadedCounting)
{
	std::vector<std::thread> threads;
	threads.resize(std::thread::hardware_concurrency());
	static std::atomic<unsigned int> counter;
	counter = 0;

	auto counter_func = []() {
		counter++;
	};

	auto create_counter_task_func = [=]() {
		scheduler->post_task_prio(counter_func, "SchedulerMultithreadedCounting");
	};

	unsigned int test_iterations = 100;

	for (unsigned int iter = 0; iter < test_iterations; ++iter)
	{
		for (size_t i = 0; i < threads.size(); ++i)
		{
			threads[i] = std::thread(create_counter_task_func);
			scheduler->run();
		}

		for (auto &thread : threads)
			thread.join();
		
		scheduler->run();
	}

	delay_ms(100);
	scheduler->run();

	CHECK_EQUAL(std::thread::hardware_concurrency() * test_iterations, counter);
}

TEST(Scheduler, SchedulerMultithreadedCountingCancelled)
{
	std::vector<std::thread> threads;
	threads.resize(std::thread::hardware_concurrency());
	static std::atomic<unsigned int> counter;
	counter = 0;

	auto counter_func = []() {
		counter++;
	};

	auto create_counter_task_func = [=]() {
		auto handle = scheduler->post_task_prio(counter_func, "SchedulerMultithreadedCountingCancelled");
		scheduler->cancel_task(handle);
	};

	unsigned int test_iterations = 100;

	for (unsigned int iter = 0; iter < test_iterations; ++iter)
	{
		for (size_t i = 0; i < threads.size(); ++i)
			threads[i] = std::thread(create_counter_task_func);

		for (auto &thread : threads)
			thread.join();
		
		scheduler->run();
	}

	delay_ms(100);
	scheduler->run();

	CHECK_EQUAL(0, counter);
}

TEST(Scheduler, SchedulerMultithreadedCountingDeferred)
{
	std::vector<std::thread> threads;
	threads.resize(std::thread::hardware_concurrency());
	static std::atomic<unsigned int> counter;
	counter = 0;

	auto counter_func = []() {
		counter++;
	};

	auto create_counter_task_func = [=]() {
		scheduler->post_task_prio(counter_func, "SchedulerMultithreadedCountingDeferred", 5);
	};

	unsigned int test_iterations = 100;

	for (unsigned int iter = 0; iter < test_iterations; ++iter)
	{
		for (size_t i = 0; i < threads.size(); ++i)
			threads[i] = std::thread(create_counter_task_func);

		for (auto &thread : threads)
			thread.join();
		
		scheduler->run();
	}

	delay_ms(100);
	scheduler->run();

	CHECK_EQUAL(std::thread::hardware_concurrency() * test_iterations, counter);
}

TEST(Scheduler, SchedulerMultithreadedCountingDeferredCancelled)
{
	std::vector<std::thread> threads;
	unsigned int simultaneous_threads = 16;  // Must be a multiple of 2 to work
	threads.resize(simultaneous_threads);
	static std::atomic<unsigned int> counter;
	counter = 0;

	auto counter_func = []() {
		counter++;
	};

	auto create_counter_task_func = [=]() {
		auto handle = scheduler->post_task_prio(counter_func, "SchedulerMultithreadedCountingDeferredCancelled", 5);
		scheduler->cancel_task(handle);
	};

	unsigned int test_iterations = 100;

	for (unsigned int iter = 0; iter < test_iterations; ++iter)
	{
		for (size_t i = 0; i < threads.size(); ++i)
			threads[i] = std::thread(create_counter_task_func);

		for (auto &thread : threads)
			thread.join();
		
		scheduler->run();
	}

	delay_ms(100);
	scheduler->run();

	CHECK_EQUAL(0, counter);
}

TEST(Scheduler, SchedulerMultithreadedCountingHalfCancelled)
{
	std::vector<std::thread> threads;
	unsigned int simultaneous_threads = 16;  // Must be a multiple of 2 to work
	threads.resize(simultaneous_threads);
	static std::atomic<unsigned int> counter;
	counter = 0;

	auto counter_func = []() {
		counter++;
	};

	auto create_counter_task_func = [=]() {
		scheduler->post_task_prio(counter_func, "SchedulerMultithreadedCountingHalfCancelled");
	};

	auto create_counter_task_func_then_cancel = [=]() {
		auto handle = scheduler->post_task_prio(counter_func, "SchedulerMultithreadedCountingHalfCancelled");
		scheduler->cancel_task(handle);
	};

	unsigned int test_iterations = 100;

	for (unsigned int iter = 0; iter < test_iterations; ++iter)
	{
		for (size_t i = 0; i < threads.size(); ++i)
		{
			if (i % 2)
				threads[i] = std::thread(create_counter_task_func);
			else
				threads[i] = std::thread(create_counter_task_func_then_cancel);
		}

		for (auto &thread : threads)
			thread.join();
		
		scheduler->run();
	}

	delay_ms(100);
	scheduler->run();

	CHECK_EQUAL(simultaneous_threads * test_iterations / 2, counter);
}

TEST(Scheduler, SchedulerMultithreadedCountingDeferredHalfCancelled)
{
	std::vector<std::thread> threads;
	unsigned int simultaneous_threads = 16;  // Must be a multiple of 2 to work
	threads.resize(simultaneous_threads);
	static std::atomic<unsigned int> counter;
	counter = 0;

	auto counter_func = []() {
		counter++;
	};

	auto create_counter_task_func = [=]() {
		scheduler->post_task_prio(counter_func, "SchedulerMultithreadedCountingDeferredHalfCancelled", 5);
	};

	auto create_counter_task_func_then_cancel = [=]() {
		auto handle = scheduler->post_task_prio(counter_func, "SchedulerMultithreadedCountingDeferredHalfCancelled", 5);
		scheduler->cancel_task(handle);
	};

	unsigned int test_iterations = 100;

	for (unsigned int iter = 0; iter < test_iterations; ++iter)
	{
		for (size_t i = 0; i < threads.size(); ++i)
		{
			if (i % 2)
				threads[i] = std::thread(create_counter_task_func);
			else
				threads[i] = std::thread(create_counter_task_func_then_cancel);
		}

		for (auto &thread : threads)
			thread.join();
		
		scheduler->run();
	}

	delay_ms(100);
	scheduler->run();

	CHECK_EQUAL(simultaneous_threads * test_iterations / 2, counter);
}
