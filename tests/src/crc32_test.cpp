#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <iostream>
#include "crc32.hpp"

using namespace std::literals::string_literals;


TEST_GROUP(CRC32)
{
};

TEST(CRC32, SimpleCRC32Test)
{
	// Refer to https://crccalc.com/?crc=123456789&method=crc32&datatype=ascii&outtype=hex
	// for the reference output vector
	CRC32 c;
	std::string data = "123456789";
	uint32_t crc = 0xFFFFFFFF;
	c.checksum((uint8_t *)data.c_str(), data.size(), crc);
	CHECK_EQUAL(0xCBF43926, crc);
}
