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


TEST(CRC8, ForcedAlignedCRC8Test)
{
	std::string data = "\x00\x00\x07\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x93"s;
	unsigned char crc = CRC8::checksum(data, 8*data.size());
	CHECK_EQUAL(0xC4, crc);
}

TEST(CRC8, MisalignedCRC8Test)
{
	std::string data = "\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xF2\x75\x4F\xD8"s;
	unsigned char crc = CRC8::checksum(data, 91);
	CHECK_EQUAL(0xC4, crc);
}

TEST(CRC8, ZeroPadTest)
{
	std::string data = "\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xF2\x75\x4F\xD8"s;
	std::string expected = "\x00\x00\x07\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x93"s;
	std::string padded = CRC8::zeropad(data, 91);
	CHECK_EQUAL(expected, padded);
}
