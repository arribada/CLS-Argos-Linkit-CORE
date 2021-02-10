#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <cmath>
#include "nrf_timer.hpp"
#include "bsp.hpp"

static constexpr uint16_t RTC_TIMER_PRESCALER = 32;

#define TICKS_TO_MS(ticks)  (((ticks) * 1000000ULL) / 992969ULL);
#define MS_TO_TICKS(ms)     (((ms) * 992969ULL) / 1000000ULL);

namespace BSP
{
	const RTC_InitTypeDefAndInst_t RTC_Inits[RTC_TOTAL_NUMBER] = {};
}

NrfTimer& nrf_timer = NrfTimer::get_instance();

TEST_GROUP(NrfTimer)
{
	void setup() {
		srand(time(NULL));

		nrf_timer.init();
	}

	void teardown() {
		nrf_timer.uninit();
	}
};

TEST(NrfTimer, Disabled)
{
	nrf_timer.stop();

	for (unsigned int i = 0; i < 10000; ++i)
	{
		CHECK_EQUAL(0, nrf_timer.get_counter());
		nrfx_rtc_increment_tick();
	}
}

TEST(NrfTimer, StartStop)
{
	nrf_timer.stop();

	for (unsigned int i = 0; i < 10000; ++i)
	{
		CHECK_EQUAL(0, nrf_timer.get_counter());
		nrfx_rtc_increment_tick();
	}

	nrf_timer.start();

	for (unsigned int i = 0; i < 10000; ++i)
	{
		uint64_t x = TICKS_TO_MS(i);
		CHECK_EQUAL( x, nrf_timer.get_counter());
		nrfx_rtc_increment_tick();
	}

	nrf_timer.stop();

	for (unsigned int i = 0; i < 10000; ++i)
	{
		uint64_t x = TICKS_TO_MS(10000);
		CHECK_EQUAL(x, nrf_timer.get_counter());
		nrfx_rtc_increment_tick();
	}
}

TEST(NrfTimer, 10000Ticks)
{
	nrf_timer.start();

	for (unsigned int i = 0; i < 10000; ++i)
	{
		uint64_t x = TICKS_TO_MS(i);
		CHECK_EQUAL( x, nrf_timer.get_counter());
		nrfx_rtc_increment_tick();
	}
}

TEST(NrfTimer, MultipleOverflows)
{
	nrf_timer.start();

	for (unsigned int i = 0; i < std::pow(2, 26); ++i)
	{
		uint64_t x = TICKS_TO_MS(i);
		CHECK_EQUAL( x, nrf_timer.get_counter());
		nrfx_rtc_increment_tick();
	}
}

TEST(NrfTimer, Schedule5ms)
{
	nrf_timer.start();

	static bool has_fired = false;

	nrf_timer.add_schedule([]() { has_fired = true; }, 5);
	
	CHECK_FALSE(has_fired);

	for (unsigned int i = 0; i < 10; ++i)
	{
		uint64_t x = TICKS_TO_MS(i);
		CHECK_EQUAL( x, nrf_timer.get_counter());
		nrfx_rtc_increment_tick();
	}

	CHECK_TRUE(has_fired);
}

TEST(NrfTimer, ScheduleAfterOverflow)
{
	nrf_timer.start();

	static bool has_fired = false;

	nrf_timer.add_schedule([]() { has_fired = true; }, std::pow(2, 25));
	
	CHECK_FALSE(has_fired);

	for (unsigned int i = 0; i < std::pow(2, 25) + 100; ++i)
	{
		uint64_t x = TICKS_TO_MS(i);
		CHECK_EQUAL( x, nrf_timer.get_counter());
		nrfx_rtc_increment_tick();
	}

	CHECK_TRUE(has_fired);
}

TEST(NrfTimer, ScheduleOrdering)
{
	nrf_timer.start();

	static bool has_fired_1 = false;
	static bool has_fired_2 = false;
	static bool has_fired_3 = false;
	static bool has_fired_4 = false;
	static bool has_fired_5 = false;

	nrf_timer.add_schedule([]() { has_fired_1 = true; }, std::pow(2, 24) );
	nrf_timer.add_schedule([]() { has_fired_2 = true; }, std::pow(2, 24) + 1);
	nrf_timer.add_schedule([]() { has_fired_3 = true; }, std::pow(2, 24) + 2);
	nrf_timer.add_schedule([]() { has_fired_4 = true; }, std::pow(2, 24) + 3);
	nrf_timer.add_schedule([]() { has_fired_5 = true; }, std::pow(2, 24) + 4);
	
	CHECK_FALSE(has_fired_1);
	CHECK_FALSE(has_fired_2);
	CHECK_FALSE(has_fired_3);
	CHECK_FALSE(has_fired_4);
	CHECK_FALSE(has_fired_5);

	while (!has_fired_1)
		nrfx_rtc_increment_tick();

	CHECK_TRUE(has_fired_1);
	CHECK_FALSE(has_fired_2);
	CHECK_FALSE(has_fired_3);
	CHECK_FALSE(has_fired_4);
	CHECK_FALSE(has_fired_5);

	while (!has_fired_2)
		nrfx_rtc_increment_tick();

	CHECK_TRUE(has_fired_1);
	CHECK_TRUE(has_fired_2);
	CHECK_FALSE(has_fired_3);
	CHECK_FALSE(has_fired_4);
	CHECK_FALSE(has_fired_5);

	while (!has_fired_3)
		nrfx_rtc_increment_tick();

	CHECK_TRUE(has_fired_1);
	CHECK_TRUE(has_fired_2);
	CHECK_TRUE(has_fired_3);
	CHECK_FALSE(has_fired_4);
	CHECK_FALSE(has_fired_5);

	while (!has_fired_4)
		nrfx_rtc_increment_tick();

	CHECK_TRUE(has_fired_1);
	CHECK_TRUE(has_fired_2);
	CHECK_TRUE(has_fired_3);
	CHECK_TRUE(has_fired_4);
	CHECK_FALSE(has_fired_5);

	while (!has_fired_5)
		nrfx_rtc_increment_tick();

	CHECK_TRUE(has_fired_1);
	CHECK_TRUE(has_fired_2);
	CHECK_TRUE(has_fired_3);
	CHECK_TRUE(has_fired_4);
	CHECK_TRUE(has_fired_5);
}

TEST(NrfTimer, CancelSingleWithMulti)
{
	nrf_timer.start();

	static bool has_fired[2] = {false};

	auto timer_handle = nrf_timer.add_schedule([]() { has_fired[0] = true; }, nrf_timer.get_counter() + 2);
	nrf_timer.add_schedule([]() { has_fired[1] = true; }, nrf_timer.get_counter() + 5);

	nrf_timer.cancel_schedule(timer_handle);
	
	for (unsigned int i = 0; i < 15; ++i)
		nrfx_rtc_increment_tick();

	CHECK_FALSE(has_fired[0]);
	CHECK_TRUE(has_fired[1]);
}

TEST(NrfTimer, Cancel)
{
	nrf_timer.start();

	static bool has_fired = false;

	auto timer_handle = nrf_timer.add_schedule([]() { has_fired = true; }, nrf_timer.get_counter() + 5);

	nrf_timer.cancel_schedule(timer_handle);

	for (unsigned int i = 0; i < 15; ++i)
		nrfx_rtc_increment_tick();

	CHECK_FALSE(has_fired);
}
