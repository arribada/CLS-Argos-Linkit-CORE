#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <iostream>
#include "crc16.hpp"

using namespace std::literals::string_literals;


TEST_GROUP(CRC16)
{
};

TEST(CRC16, SimpleCRC16Test)
{
	std::string data = "\x01\x23\x45\x67\x89\x62\x82"s;
	unsigned int crc = CRC16::checksum(data, data.size()*8);
	CHECK_EQUAL(0, crc);
}
