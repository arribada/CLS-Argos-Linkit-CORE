#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "timer.hpp"
#include <iostream>

using namespace std;

static void dummy_function(void *) {}

TEST_GROUP(Timer)
{
	void setup() {
		Timer::start();
	}

	void teardown() {
		Timer::stop();
	}
};


TEST(Timer, TimerCounterIsAdvancing)
{
	uint64_t t = Timer::get_counter();
	usleep(4000);
	CHECK_COMPARE(t, <, Timer::get_counter());
}

TEST(Timer, TimerEventIsFired)
{
	static bool has_fired = false;
	static void *param = 0;
	TimerSchedule s = {
		(void *)1,
		[](void *a) { has_fired = true; param = a; },
		Timer::get_counter() + 5
	};
	Timer::add_schedule(s);
	usleep(15000);
	Timer::cancel_schedule(s);
	CHECK_TRUE(has_fired);
	CHECK_EQUAL(s.user_arg, param);
}

TEST(Timer, TimerMultiEventIsFired)
{
	static bool has_fired[2] = {false};
	static void *params[2] = {0};
	TimerSchedule s0 = {
		(void *)1,
		[](void *a) { has_fired[0] = true; params[0] = a; },
		Timer::get_counter() + 2
	};
	TimerSchedule s1 = {
		(void *)2,
		[](void *a) { has_fired[1] = true; params[1] = a; },
		Timer::get_counter() + 5
	};
	Timer::add_schedule(s0);
	Timer::add_schedule(s1);
	usleep(15000);
	CHECK_TRUE(has_fired[0]);
	CHECK_TRUE(has_fired[1]);
	CHECK_EQUAL(s0.user_arg, params[0]);
	CHECK_EQUAL(s1.user_arg, params[1]);
}

TEST(Timer, TimerCancelSingleWithMulti)
{
	static bool has_fired[2] = {false};
	static void *params[2] = {0};
	TimerSchedule s0 = {
		(void *)1,
		[](void *a) { has_fired[0] = true; params[0] = a; },
		Timer::get_counter() + 2
	};
	TimerSchedule s1 = {
		(void *)2,
		[](void *a) { has_fired[1] = true; params[1] = a; },
		Timer::get_counter() + 5
	};
	Timer::add_schedule(s0);
	Timer::add_schedule(s1);
	Timer::cancel_schedule(s0);
	usleep(15000);
	CHECK_FALSE(has_fired[0]);
	CHECK_TRUE(has_fired[1]);
}

TEST(Timer, TimerCancelBySchedule)
{
	static bool has_fired = false;
	static void *param = 0;
	TimerSchedule s = {
		(void *)1,
		[](void *a) { has_fired = true; param = a; },
		Timer::get_counter() + 5
	};
	Timer::add_schedule(s);
	Timer::cancel_schedule(s);
	usleep(15000);
	CHECK_FALSE(has_fired);
}

TEST(Timer, TimerCancelByUserArg)
{
	static bool has_fired = false;
	static void *param = 0;
	TimerSchedule s = {
		(void *)1,
		[](void *a) { has_fired = true; param = a; },
		Timer::get_counter() + 5
	};
	Timer::add_schedule(s);
	Timer::cancel_schedule(s.user_arg);
	usleep(15000);
	CHECK_FALSE(has_fired);
}

TEST(Timer, TimerCancelByFunction)
{
	static bool has_fired = false;
	static void *param = 0;
	TimerSchedule s = {
		(void *)1,
		dummy_function,
		Timer::get_counter() + 5
	};
	Timer::add_schedule(s);
	Timer::cancel_schedule(dummy_function);
	usleep(15000);
	CHECK_FALSE(has_fired);
}
