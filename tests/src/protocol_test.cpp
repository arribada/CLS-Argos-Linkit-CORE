#include "dte_protocol.hpp"
#include "debug.hpp"
#include "error.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


TEST_GROUP(DTEProtocol)
{
	std::vector<ParamValue> values = {
		{ ParamID::ARGOS_HEXID, 0xDEADU },
		{ ParamID::ARGOS_DECID, 0xDEADU },
	};
	std::vector<ParamID> params = {
		ParamID::ARGOS_HEXID,
		ParamID::ARGOS_DECID,
		ParamID::DEVICE_MODEL,
		ParamID::FW_APP_VERSION,
		ParamID::LAST_TX,
		ParamID::TX_COUNTER,
		ParamID::BATT_SOC,
		ParamID::LAST_FULL_CHARGE_DATE,
		ParamID::PROFILE_NAME,
		ParamID::ARGOS_AOP_DATE,
		ParamID::ARGOS_FREQ,
		ParamID::ARGOS_POWER,
		ParamID::TR_NOM,
	};
	void setup() {
	}
	void teardown() {
	}
};

TEST(DTEProtocol, PARML_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARML_REQ);
	CHECK_EQUAL("$PARML#000;\r", s);
}

TEST(DTEProtocol, PARML_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARML_RESP, params);
	CHECK_EQUAL("$O;PARML#04D;IDT07,IDT06,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP09,ART03,ARP03,ARP04,ARP05\r", s);
}

TEST(DTEProtocol, PARMR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMR_REQ, params);
	CHECK_EQUAL("$PARMR#04D;IDT07,IDT06,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP09,ART03,ARP03,ARP04,ARP05\r", s);
}

TEST(DTEProtocol, PARMW_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMW_REQ, values);
	CHECK_EQUAL("$PARMW#016;IDT07=DEAD,IDT06=57005\r", s);
}

TEST(DTEProtocol, ZONEW_REQ)
{
	std::string s;
	// Convert a buffer to base64
	unsigned char buffer[] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer) };
	s = DTEEncoder::encode(DTECommand::ZONEW_REQ, raw_data);
	CHECK_EQUAL("$ZONEW#014;AAECAwQFBgcJCgsMDQ4P\r", s);
}

TEST(DTEProtocol, PROFR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFR_REQ);
	CHECK_EQUAL("$PROFR#000;\r", s);
}

TEST(DTEProtocol, PROFR_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFR_RESP, 0, std::string("Underwater Vehicle"));
	CHECK_EQUAL("$O;PROFR#012;Underwater Vehicle\r", s);
}

TEST(DTEProtocol, PROFW_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFW_REQ, std::string("Underwater Vehicle"));
	CHECK_EQUAL("$PROFW#012;Underwater Vehicle\r", s);
}

TEST(DTEProtocol, PROFW_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFW_RESP, 0);
	CHECK_EQUAL("$O;PROFW#000;\r", s);
}

TEST(DTEProtocol, PASPW_REQ)
{
	std::string s;
	unsigned char buffer[] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer) };
	s = DTEEncoder::encode(DTECommand::PASPW_REQ, raw_data);
	CHECK_EQUAL("$PASPW#014;AAECAwQFBgcJCgsMDQ4P\r", s);
}

TEST(DTEProtocol, PASPW_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PASPW_RESP, 0);
	CHECK_EQUAL("$O;PASPW#000;\r", s);
}

TEST(DTEProtocol, SECUR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::SECUR_REQ);
	CHECK_EQUAL("$SECUR#000;\r", s);
}

TEST(DTEProtocol, SECUR_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::SECUR_RESP, 0);
	CHECK_EQUAL("$O;SECUR#000;\r", s);
}

TEST(DTEProtocol, DUMPM_REQ)
{
	std::string s;
	// Command with multi-args
	s = DTEEncoder::encode(DTECommand::DUMPM_REQ, 0x10000, 0x200);
	CHECK_EQUAL("$DUMPM#009;10000,200\r", s);

	// Surplus args are ignored
	s = DTEEncoder::encode(DTECommand::DUMPM_REQ, 0x10000, 0x200, 1, 2, 3);
	CHECK_EQUAL("$DUMPM#009;10000,200\r", s);
}

TEST(DTEProtocol, DUMPM_RESP)
{
	std::string s;
	unsigned char buffer[0x200] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer) };
	s = DTEEncoder::encode(DTECommand::DUMPM_RESP, 0, raw_data);
	CHECK_EQUAL("$O;DUMPM#2AC;AAECAwQFBgcJCgsMDQ4PAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\r", s);
}

TEST(DTEProtocol, DUMPM_RESP_ERROR)
{
	std::string s;
	// Command response with an error code
	s = DTEEncoder::encode(DTECommand::DUMPM_RESP, 3);
	CHECK_EQUAL("$N;DUMPM#001;3\r", s);
}

TEST(DTEProtocol, DUMPL_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::DUMPL_REQ);
	CHECK_EQUAL("$DUMPL#000;\r", s);
}

TEST(DTEProtocol, DUMPL_RESP)
{
	std::string s;
	unsigned char buffer[0x200] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer) };
	s = DTEEncoder::encode(DTECommand::DUMPL_RESP, 0, raw_data);
	CHECK_EQUAL("$O;DUMPL#2AC;AAECAwQFBgcJCgsMDQ4PAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\r", s);
}

TEST(DTEProtocol, DUMPD_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::DUMPD_REQ);
	CHECK_EQUAL("$DUMPD#000;\r", s);
}

TEST(DTEProtocol, DUMPD_RESP)
{
	std::string s;
	unsigned char buffer[0x200] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer) };
	s = DTEEncoder::encode(DTECommand::DUMPD_RESP, 0, raw_data);
	CHECK_EQUAL("$O;DUMPD#2AC;AAECAwQFBgcJCgsMDQ4PAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\r", s);
}

TEST(DTEProtocol, FACTR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::FACTR_REQ);
	CHECK_EQUAL("$FACTR#000;\r", s);
}

TEST(DTEProtocol, FACTR_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::FACTR_RESP, 0);
	CHECK_EQUAL("$O;FACTR#000;\r", s);
}

TEST(DTEProtocol, RESET_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::RESET_REQ);
	CHECK_EQUAL("$RESET#000;\r", s);
}

TEST(DTEProtocol, RESET_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::RESET_RESP, 0);
	CHECK_EQUAL("$O;RESET#000;\r", s);
}

TEST(DTEProtocol, PARAM_ARGOS_DECID_RANGE)
{
	ParamValue p = { ParamID::ARGOS_DECID, 0xFFFFFFFFU };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(DTEProtocol, PARAM_ARGOS_HEXID_RANGE)
{
	ParamValue p = { ParamID::ARGOS_HEXID, 0xFFFFFFFFU };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(DTEProtocol, PARAM_ARGOS_DEVICE_MODEL_RANGE)
{
	ParamValue p = { ParamID::DEVICE_MODEL, 0x100U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(DTEProtocol, PARAM_ARGOS_FW_APP_VERSION)
{
	std::string s;
	ParamValue p = { ParamID::FW_APP_VERSION, "V1.2.3.4" };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#00E;IDT03=V1.2.3.4\r", s);
}

TEST(DTEProtocol, PARAM_ARGOS_FW_APP_VERSION_RANGE)
{
	ParamValue p = { ParamID::FW_APP_VERSION, "V1.2.3.4xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(DTEProtocol, PARAM_LAST_TX)
{
	std::string s;
	ParamValue p = { ParamID::LAST_TX, (std::time_t)0 };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#01E;ART01=Thu Jan  1 00:00:00 1970\r", s);
}
