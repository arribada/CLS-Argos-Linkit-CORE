#include "dte_protocol.hpp"
#include "debug.hpp"
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


TEST_GROUP(DTEProtocol)
{
	void setup() {
	}
	void teardown() {
	}
};

TEST(DTEProtocol, Sanity)
{
	std::string s;

	s = DTEEncoder::encode(DTECommand::PARML_REQ);
	DEBUG_TRACE(s.c_str());

	s = DTEEncoder::encode(DTECommand::PARMR_REQ, ParamID::ARGOS_HEXID, ParamID::ARGOS_DECID, ParamID::__NULL_PARAM);
	DEBUG_TRACE(s.c_str());

	ParamValue end_of_list = { ParamID::__NULL_PARAM };
	ParamValue values[] = {
		{ ParamID::ARGOS_HEXID, 0xDEADU },
		{ ParamID::ARGOS_DECID, 0xDEADU },
	};

	s = DTEEncoder::encode(DTECommand::PARMW_REQ, values[0], values[1], end_of_list);
	DEBUG_TRACE(s.c_str());

	unsigned char buffer[] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	DTEBase64 req = { buffer, sizeof(buffer) };
	s = DTEEncoder::encode(DTECommand::ZONEW_REQ, req);
	DEBUG_TRACE(s.c_str());

}
