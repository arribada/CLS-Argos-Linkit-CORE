#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "linux_timer.hpp"
#include <iostream>

Timer *timer = new LinuxTimer;

TEST_GROUP(Timer)
{
	void setup() {
		timer->start();
	}

	void teardown() {
		timer->stop();
	}
};

TEST(Timer, TimerCounterIsAdvancing)
{
	uint64_t t = timer->get_counter();

	// Make sure the timer has advanced sufficiently to schedule all events
	while (timer->get_counter() < 5)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

	CHECK_COMPARE(t, <, timer->get_counter());
}

TEST(Timer, TimerEventIsFired)
{
	static bool has_fired = false;

	timer->add_schedule([]() { has_fired = true; }, timer->get_counter() + 5);

	// Make sure the timer has advanced sufficiently to schedule all events
	while (timer->get_counter() < 5)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

	CHECK_TRUE(has_fired);
}

TEST(Timer, TimerMultiEventIsFired)
{
	static bool has_fired[2] = {false};

	timer->add_schedule([]() { has_fired[0] = true; }, timer->get_counter() + 2);
	timer->add_schedule([]() { has_fired[1] = true; }, timer->get_counter() + 5);

	// Make sure the timer has advanced sufficiently to schedule all events
	while (timer->get_counter() < 5)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

	CHECK_TRUE(has_fired[0]);
	CHECK_TRUE(has_fired[1]);
}

TEST(Timer, TimerCancelSingleWithMulti)
{
	static bool has_fired[2] = {false};

	auto timer_handle = timer->add_schedule([]() { has_fired[0] = true; }, timer->get_counter() + 2);
	timer->add_schedule([]() { has_fired[1] = true; }, timer->get_counter() + 5);

	timer->cancel_schedule(timer_handle);
	std::this_thread::sleep_for(std::chrono::milliseconds(15));
	CHECK_FALSE(has_fired[0]);
	CHECK_TRUE(has_fired[1]);
}

TEST(Timer, TimerCancel)
{
	static bool has_fired = false;

	auto timer_handle = timer->add_schedule([]() { has_fired = true; }, timer->get_counter() + 5);

	timer->cancel_schedule(timer_handle);
	std::this_thread::sleep_for(std::chrono::milliseconds(15));
	CHECK_FALSE(has_fired);
}
