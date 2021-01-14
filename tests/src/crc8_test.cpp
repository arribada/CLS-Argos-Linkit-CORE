#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <iostream>
#include "crc8.hpp"

using namespace std::literals::string_literals;


TEST_GROUP(CRC8)
{
};

TEST(CRC8, SimpleCRC8Test)
{
	std::string data = "123456789";
	unsigned char crc = CRC8::checksum(data, data.size()*8);
	CHECK_EQUAL(0xF4, crc);
}
