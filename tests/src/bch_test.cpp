#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <iostream>
#include "bch.hpp"

using namespace std::literals::string_literals;


TEST_GROUP(BCH)
{
};

TEST(BCH, LongMessageReferenceSpec)
{
	std::string data = "\x8B\xE3\x81\xA9\x49\xC0\x39\xFE\x03\xBE\xA9\x52\x93\xB0\x73\xF4\x2A\xD2\x30\x0E\x79\x85\x5A\x46\x81\xCF\x34"s;
	BCHCodeWord code_word = BCHEncoder::encode(BCHEncoder::B255_223_4, sizeof(BCHEncoder::B255_223_4), data, data.size()*8);
	CHECK_EQUAL(0x89A3AAC9, code_word);
}

TEST(BCH, ShortMessageReferenceSpec)
{
	std::string data = "\x11\x3B\xC6\x3E\xA7\xFC\x01\x1B\xE0\x00\x00\x10\x80"s;
	BCHCodeWord code_word = BCHEncoder::encode(BCHEncoder::B127_106_3, sizeof(BCHEncoder::B127_106_3), data, 99);
	// TODO: need reference vector for short message BCH
	CHECK_EQUAL(0x5D786, code_word);
}

TEST(BCH, JoshuaReference1)
{
	std::string data = "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"s;
	BCHCodeWord code_word = BCHEncoder::encode(BCHEncoder::B127_106_3, sizeof(BCHEncoder::B127_106_3), data, 99);
	// TODO: need reference vector for short message BCH
	CHECK_EQUAL(0x1F0624, code_word);
}


TEST(BCH, JoshuaReference2)
{
	std::string data = "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55"s;
	BCHCodeWord code_word = BCHEncoder::encode(BCHEncoder::B127_106_3, sizeof(BCHEncoder::B127_106_3), data, 99);
	// TODO: need reference vector for short message BCH
	CHECK_EQUAL(0x1F8312, code_word);
}

TEST(BCH, JoshuaReference3)
{
	std::string data = "\x67\xC6\x69\x73\x51\xFF\x4A\xEC\x29\xCD\xBA\xAB\xE0"s;
	BCHCodeWord code_word = BCHEncoder::encode(BCHEncoder::B127_106_3, sizeof(BCHEncoder::B127_106_3), data, 99);
	// TODO: need reference vector for short message BCH
	CHECK_EQUAL(0xE5DF8, code_word);
}
