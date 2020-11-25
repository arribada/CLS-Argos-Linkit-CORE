#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "linux_timer.hpp"
#include <iostream>

using namespace std;

Timer *timer = new LinuxTimer;

static void dummy_function(void *) {}

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
	usleep(4000);
	CHECK_COMPARE(t, <, timer->get_counter());
}

TEST(Timer, TimerEventIsFired)
{
	static bool has_fired = false;
	static void *param = 0;
	TimerSchedule s = {
		(void *)1,
		[](void *a) { has_fired = true; param = a; },
		timer->get_counter() + 5
	};
	timer->add_schedule(s);
	usleep(15000);
	timer->cancel_schedule(s);
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
		timer->get_counter() + 2
	};
	TimerSchedule s1 = {
		(void *)2,
		[](void *a) { has_fired[1] = true; params[1] = a; },
		timer->get_counter() + 5
	};
	timer->add_schedule(s0);
	timer->add_schedule(s1);
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
		timer->get_counter() + 2
	};
	TimerSchedule s1 = {
		(void *)2,
		[](void *a) { has_fired[1] = true; params[1] = a; },
		timer->get_counter() + 5
	};
	timer->add_schedule(s0);
	timer->add_schedule(s1);
	timer->cancel_schedule(s0);
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
		timer->get_counter() + 5
	};
	timer->add_schedule(s);
	timer->cancel_schedule(s);
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
		timer->get_counter() + 5
	};
	timer->add_schedule(s);
	timer->cancel_schedule(s.user_arg);
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
		timer->get_counter() + 5
	};
	timer->add_schedule(s);
	timer->cancel_schedule(dummy_function);
	usleep(15000);
	CHECK_FALSE(has_fired);
}
