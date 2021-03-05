#include "dte_protocol.hpp"
#include "debug.hpp"
#include "error.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

using namespace std::literals::string_literals;



TEST_GROUP(Encoder)
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

TEST(Encoder, PARML_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARML_REQ);
	STRCMP_EQUAL("$PARML#000;\r", s.c_str());
}

TEST(Encoder, PARML_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARML_RESP, params);
	STRCMP_EQUAL("$O;PARML#04D;IDT07,IDT06,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP11,ART03,ARP03,ARP04,ARP05\r", s.c_str());
}

TEST(Encoder, PARMR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMR_REQ, params);
	STRCMP_EQUAL("$PARMR#04D;IDT07,IDT06,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP11,ART03,ARP03,ARP04,ARP05\r", s.c_str());
}

TEST(Encoder, STATR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::STATR_REQ, params);
	STRCMP_EQUAL("$STATR#04D;IDT07,IDT06,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP11,ART03,ARP03,ARP04,ARP05\r", s.c_str());
}

TEST(Encoder, PARMW_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMW_REQ, values);
	STRCMP_EQUAL("$PARMW#016;IDT07=DEAD,IDT06=57005\r", s.c_str());
}

TEST(Encoder, ZONER_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::ZONER_REQ, 0x1);
	STRCMP_EQUAL("$ZONER#001;1\r", s.c_str());
}

TEST(Encoder, ZONER_RESP)
{
	std::string s;
	BaseRawData raw_data;
	raw_data.str = "\x00\x01\x02\x03\x04\x05\x06\x07\x09\x0A\x0B\x0C\x0D\x0E\x0F"s;
	raw_data.length = 0;
	s = DTEEncoder::encode(DTECommand::ZONER_RESP, 0, raw_data);
	STRCMP_EQUAL("$O;ZONER#014;AAECAwQFBgcJCgsMDQ4P\r", s.c_str());
}

TEST(Encoder, ZONER_REQ_OutOfRangeCheck)
{
	std::string s;
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::ZONER_REQ, 0xA));  // Only 1 zone supported with ID=0x01
}

TEST(Encoder, ZONEW_REQ)
{
	std::string s;
	// Convert a buffer to base64
	BaseRawData raw_data;
	raw_data.str = "\x00\x01\x02\x03\x04\x05\x06\x07\x09\x0A\x0B\x0C\x0D\x0E\x0F"s;
	raw_data.length = 0;
	s = DTEEncoder::encode(DTECommand::ZONEW_REQ, raw_data);
	STRCMP_EQUAL("$ZONEW#014;AAECAwQFBgcJCgsMDQ4P\r", s.c_str());
}

TEST(Encoder, PROFR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFR_REQ);
	STRCMP_EQUAL("$PROFR#000;\r", s.c_str());
}

TEST(Encoder, PROFR_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFR_RESP, 0, std::string("Underwater Vehicle"));
	STRCMP_EQUAL("$O;PROFR#012;Underwater Vehicle\r", s.c_str());
}

TEST(Encoder, PROFW_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFW_REQ, std::string("Underwater Vehicle"));
	STRCMP_EQUAL("$PROFW#012;Underwater Vehicle\r", s.c_str());
}

TEST(Encoder, PROFW_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFW_RESP, 0);
	STRCMP_EQUAL("$O;PROFW#000;\r", s.c_str());
}

TEST(Encoder, PASPW_REQ)
{
	std::string s;
	unsigned char buffer[] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer), "" };
	s = DTEEncoder::encode(DTECommand::PASPW_REQ, raw_data);
	STRCMP_EQUAL("$PASPW#014;AAECAwQFBgcJCgsMDQ4P\r", s.c_str());
}

TEST(Encoder, PASPW_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PASPW_RESP, 0);
	STRCMP_EQUAL("$O;PASPW#000;\r", s.c_str());
}

TEST(Encoder, SECUR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::SECUR_REQ, 0x1234U);
	STRCMP_EQUAL("$SECUR#004;1234\r", s.c_str());
}

TEST(Encoder, SECUR_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::SECUR_RESP, 0);
	STRCMP_EQUAL("$O;SECUR#000;\r", s.c_str());
}

TEST(Encoder, DUMPM_REQ)
{
	std::string s;
	// Command with multi-args
	s = DTEEncoder::encode(DTECommand::DUMPM_REQ, 0x10000, 0x200);
	STRCMP_EQUAL("$DUMPM#009;10000,200\r", s.c_str());

	// Surplus args are ignored
	s = DTEEncoder::encode(DTECommand::DUMPM_REQ, 0x10000, 0x200, 1, 2, 3);
	STRCMP_EQUAL("$DUMPM#009;10000,200\r", s.c_str());
}

TEST(Encoder, DUMPM_RESP)
{
	std::string s;
	unsigned char buffer[0x200] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer), "" };
	s = DTEEncoder::encode(DTECommand::DUMPM_RESP, 0, raw_data);
	STRCMP_EQUAL("$O;DUMPM#2AC;AAECAwQFBgcJCgsMDQ4PAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\r", s.c_str());
}

TEST(Encoder, DUMPM_RESP_ERROR)
{
	std::string s;
	// Command response with an error code
	s = DTEEncoder::encode(DTECommand::DUMPM_RESP, 3);
	STRCMP_EQUAL("$N;DUMPM#001;3\r", s.c_str());
}

TEST(Encoder, DUMPD_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::DUMPD_REQ, BaseLogDType::INTERNAL);
	STRCMP_EQUAL("$DUMPD#001;0\r", s.c_str());
	s = DTEEncoder::encode(DTECommand::DUMPD_REQ, BaseLogDType::SENSOR);
	STRCMP_EQUAL("$DUMPD#001;1\r", s.c_str());
}

TEST(Encoder, DUMPD_RESP)
{
	std::string s;
	unsigned char buffer[0x200] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer), "" };
	s = DTEEncoder::encode(DTECommand::DUMPD_RESP, 0, 0U, 1U, raw_data);
	STRCMP_EQUAL("$O;DUMPD#2B0;0,1,AAECAwQFBgcJCgsMDQ4PAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\r", s.c_str());
}

TEST(Encoder, FACTW_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::FACTW_REQ);
	STRCMP_EQUAL("$FACTW#000;\r", s.c_str());
}

TEST(Encoder, FACTW_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::FACTW_RESP, 0);
	STRCMP_EQUAL("$O;FACTW#000;\r", s.c_str());
}

TEST(Encoder, RSTVW_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::RSTVW_REQ, 1U);
	STRCMP_EQUAL("$RSTVW#001;1\r", s.c_str());
}

TEST(Encoder, RSTVW_REQ_OutOfRangeCheck)
{
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::RSTVW_REQ, 2U));
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::RSTVW_REQ, 0U));
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::RSTVW_REQ, 3U));
}

TEST(Encoder, RSTVW_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::RSTVW_RESP, 0);
	STRCMP_EQUAL("$O;RSTVW#000;\r", s.c_str());
}

TEST(Encoder, RSTBW_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::RSTBW_REQ);
	STRCMP_EQUAL("$RSTBW#000;\r", s.c_str());
}

TEST(Encoder, RSTBW_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::RSTBW_RESP, 0);
	STRCMP_EQUAL("$O;RSTBW#000;\r", s.c_str());
}

TEST(Encoder, PARAM_ARGOS_DECID_OutOfRangeCheck)
{
	ParamValue p = { ParamID::ARGOS_DECID, 0xFFFFFFFFU };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(Encoder, PARAM_ARGOS_HEXID_OutOfRangeCheck)
{
	ParamValue p = { ParamID::ARGOS_HEXID, 0xFFFFFFFFU };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(Encoder, PARAM_ARGOS_DEVICE_MODEL_OutOfRangeCheck)
{
	ParamValue p = { ParamID::DEVICE_MODEL, "GenTrackerxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"s };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(Encoder, PARAM_ARGOS_FW_APP_VERSION)
{
	std::string s;
	ParamValue p = { ParamID::FW_APP_VERSION, "V1.2.3.4"s };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#00E;IDT03=V1.2.3.4\r", s.c_str());
}

TEST(Encoder, PARAM_ARGOS_FW_APP_VERSION_OutOfRangeCheck)
{
	ParamValue p = { ParamID::FW_APP_VERSION, "V1.2.3.4xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"s };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(Encoder, PARAM_LAST_TX)
{
	std::string s;
	ParamValue p = { ParamID::LAST_TX, (std::time_t)0 };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#01E;ART01=Thu Jan  1 00:00:00 1970\r", s.c_str());
}

TEST(Encoder, STAT_LAST_TX)
{
	std::string s;
	ParamValue p = { ParamID::LAST_TX, (std::time_t)0 };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::STATR_RESP, v);
	STRCMP_EQUAL("$O;STATR#01E;ART01=Thu Jan  1 00:00:00 1970\r", s.c_str());
}

TEST(Encoder, STAT_DEVICE_MODEL)
{
	std::string s;
	ParamValue p = { ParamID::DEVICE_MODEL, "GenTracker"s };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::STATR_RESP, v);
	STRCMP_EQUAL("$O;STATR#010;IDT02=GenTracker\r", s.c_str());
}

TEST(Encoder, PARAM_TX_COUNTER)
{
	std::string s;
	ParamValue p = { ParamID::TX_COUNTER, 22U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;ART02=22\r", s.c_str());
}

TEST(Encoder, PARAM_BATT_SOC_OutOfRangeCheck)
{
	ParamValue p = { ParamID::BATT_SOC, 101U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(Encoder, PARAM_BATT_SOC)
{
	std::string s;
	ParamValue p = { ParamID::BATT_SOC, 100U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#009;POT03=100\r", s.c_str());

	p = { ParamID::BATT_SOC, 0U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;POT03=0\r", s.c_str());
}

TEST(Encoder, PARAM_LAST_FULL_CHARGE_DATE)
{
	std::string s;
	ParamValue p = { ParamID::LAST_FULL_CHARGE_DATE, (std::time_t)0 };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#01E;POT05=Thu Jan  1 00:00:00 1970\r", s.c_str());
}

TEST(Encoder, PARAM_ARGOS_PROFILE_NAME)
{
	std::string s;
	ParamValue p = { ParamID::PROFILE_NAME, "Turtle Tracker"s };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#014;IDP11=Turtle Tracker\r", s.c_str());
}

TEST(Encoder, PARAM_ARGOS_AOP_STATUS)
{
	std::string s;
	unsigned char buffer[] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer), "" };
	ParamValue p = { ParamID::AOP_STATUS, raw_data };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#01A;XXXXX=AAECAwQFBgcJCgsMDQ4P\r", s.c_str());
}

TEST(Encoder, PARAM_ARGOS_AOP_DATE)
{
	std::string s;
	ParamValue p = { ParamID::ARGOS_AOP_DATE, (std::time_t)0 };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#01E;ART03=Thu Jan  1 00:00:00 1970\r", s.c_str());
}

TEST(Encoder, PARAM_ARGOS_FREQ)
{
	std::string s;
	ParamValue p = { ParamID::ARGOS_FREQ, 401.6599};
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#009;ARP03=399\r", s.c_str());
}

TEST(Encoder, PARAM_ARGOS_FREQ_OutOfRangeCheck)
{
	ParamValue p = { ParamID::ARGOS_FREQ, 300.0};
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::ARGOS_FREQ, 402.0};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_ARGOS_POWER)
{
	std::string s;
	ParamValue p = { ParamID::ARGOS_POWER, BaseArgosPower::POWER_3_MW};
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP04=1\r", s.c_str());
	p = { ParamID::ARGOS_POWER, BaseArgosPower::POWER_40_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP04=2\r", s.c_str());
	p = { ParamID::ARGOS_POWER, BaseArgosPower::POWER_200_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP04=3\r", s.c_str());
	p = { ParamID::ARGOS_POWER, BaseArgosPower::POWER_500_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP04=4\r", s.c_str());
}

TEST(Encoder, PARAM_TR_NOM)
{
	std::string s;
	ParamValue p = { ParamID::TR_NOM, 50U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;ARP05=50\r", s.c_str());
}

TEST(Encoder, PARAM_TR_NOM_OutOfRangeCheck)
{
	ParamValue p = { ParamID::TR_NOM, 44U};
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::TR_NOM, 1201U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_ARGOS_MODE)
{
	std::string s;
	ParamValue p = { ParamID::ARGOS_MODE, BaseArgosMode::OFF};
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP01=0\r", s.c_str());
	p = { ParamID::ARGOS_MODE, BaseArgosMode::LEGACY};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP01=1\r", s.c_str());
	p = { ParamID::ARGOS_MODE, BaseArgosMode::PASS_PREDICTION};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP01=2\r", s.c_str());
	p = { ParamID::ARGOS_MODE, BaseArgosMode::DUTY_CYCLE};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP01=3\r", s.c_str());
}


TEST(Encoder, PARAM_NTRY_PER_MESSAGE)
{
	std::string s;
	ParamValue p = { ParamID::NTRY_PER_MESSAGE, 3U};
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP19=3\r", s.c_str());
}

TEST(Encoder, PARAM_NTRY_PER_MESSAGE_OutOfRangeCheck)
{
	ParamValue p = { ParamID::NTRY_PER_MESSAGE, 86401U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_DUTY_CYCLE_OutOfRangeCheck)
{
	ParamValue p = { ParamID::DUTY_CYCLE, 0x1FFFFFFU };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_DUTY_CYCLE)
{
	std::string s;
	ParamValue p = { ParamID::DUTY_CYCLE, 0b101010101010101010101010U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#00E;ARP18=11184810\r", s.c_str());
}

TEST(Encoder, PARAM_GNSS_EN)
{
	std::string s;
	ParamValue p = { ParamID::GNSS_EN, true };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;GNP01=1\r", s.c_str());
	p = { ParamID::GNSS_EN, false };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;GNP01=0\r", s.c_str());
}

TEST(Encoder, PARAM_DLOC_ARG_NOM)
{
	std::string s;
	ParamValue p = { ParamID::DLOC_ARG_NOM, 10*60U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP11=1\r", s.c_str());
	p = { ParamID::DLOC_ARG_NOM, 15*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP11=2\r", s.c_str());
	p = { ParamID::DLOC_ARG_NOM, 30*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP11=3\r", s.c_str());
	p = { ParamID::DLOC_ARG_NOM, 60*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP11=4\r", s.c_str());
	p = { ParamID::DLOC_ARG_NOM, 120*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP11=5\r", s.c_str());
	p = { ParamID::DLOC_ARG_NOM, 360*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP11=6\r", s.c_str());
	p = { ParamID::DLOC_ARG_NOM, 720*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP11=7\r", s.c_str());
	p = { ParamID::DLOC_ARG_NOM, 1440*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP11=8\r", s.c_str());
}

TEST(Encoder, PARAM_ARGOS_DEPTH_PILE)
{
	std::string s;
	ParamValue p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_1 };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP16=1\r", s.c_str());
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_2 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP16=2\r", s.c_str());
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_3 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP16=3\r", s.c_str());
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_4 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP16=4\r", s.c_str());
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_8 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP16=8\r", s.c_str());
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_12 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP16=9\r", s.c_str());
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_16 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;ARP16=10\r", s.c_str());
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_20 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;ARP16=11\r", s.c_str());
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_24 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;ARP16=12\r", s.c_str());
}

TEST(Encoder, PARAM_GNSS_HDOPFILT_EN)
{
	std::string s;
	ParamValue p = { ParamID::GNSS_HDOPFILT_EN, true };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;GNP02=1\r", s.c_str());
	p = { ParamID::GNSS_HDOPFILT_EN, false };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;GNP02=0\r", s.c_str());
}

TEST(Encoder, PARAM_GNSS_HDOPFILT_THR_)
{
	std::string s;
	ParamValue p = { ParamID::GNSS_HDOPFILT_THR, 2U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;GNP03=2\r", s.c_str());
}

TEST(Encoder, PARAM_GNSS_HDOPFILT_THR_OutOfRangeCheck)
{
	ParamValue p = { ParamID::GNSS_HDOPFILT_THR, 1U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::GNSS_HDOPFILT_THR, 16U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_GNSS_ACQ_TIMEOUT)
{
	std::string s;
	ParamValue p = { ParamID::GNSS_ACQ_TIMEOUT, 30U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;GNP05=30\r", s.c_str());
}

TEST(Encoder, PARAM_GNSS_ACQ_TIMEOUT_OutOfRangeCheck)
{
	ParamValue p = { ParamID::GNSS_ACQ_TIMEOUT, 9U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::GNSS_ACQ_TIMEOUT, 61U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_UNDERWATER_EN)
{
	std::string s;
	ParamValue p = { ParamID::UNDERWATER_EN, true };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;UNP01=1\r", s.c_str());
	p = { ParamID::UNDERWATER_EN, false };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;UNP01=0\r", s.c_str());
}

TEST(Encoder, PARAM_DRY_TIME_BEFORE_TX)
{
	std::string s;
	ParamValue p = { ParamID::DRY_TIME_BEFORE_TX, 10U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;UNP02=10\r", s.c_str());
}

TEST(Encoder, PARAM_DRY_TIME_BEFORE_TX_OutOfRangeCheck)
{
	ParamValue p = { ParamID::DRY_TIME_BEFORE_TX, 0U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::DRY_TIME_BEFORE_TX, 1441U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_SAMPLING_UNDER_FREQ)
{
	std::string s;
	ParamValue p = { ParamID::SAMPLING_UNDER_FREQ, 10U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;UNP03=10\r", s.c_str());
}

TEST(Encoder, PARAM_SAMPLING_UNDER_FREQ_OutOfRangeCheck)
{
	ParamValue p = { ParamID::SAMPLING_UNDER_FREQ, 0U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::SAMPLING_UNDER_FREQ, 1441U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_LB_EN)
{
	std::string s;
	ParamValue p = { ParamID::LB_EN, true };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP01=1\r", s.c_str());
	p = { ParamID::LB_EN, false };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP01=0\r", s.c_str());
}

TEST(Encoder, PARAM_LB_TRESHOLD)
{
	std::string s;
	ParamValue p = { ParamID::LB_TRESHOLD, 10U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;LBP02=10\r", s.c_str());
}

TEST(Encoder, PARAM_LB_TRESHOLD_OutOfRangeCheck)
{
	ParamValue p = { ParamID::LB_TRESHOLD, 101U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}


TEST(Encoder, PARAM_LB_ARGOS_POWER)
{
	std::string s;
	ParamValue p = { ParamID::LB_ARGOS_POWER, BaseArgosPower::POWER_3_MW};
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP03=1\r", s.c_str());
	p = { ParamID::LB_ARGOS_POWER, BaseArgosPower::POWER_40_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP03=2\r", s.c_str());
	p = { ParamID::LB_ARGOS_POWER, BaseArgosPower::POWER_200_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP03=3\r", s.c_str());
	p = { ParamID::LB_ARGOS_POWER, BaseArgosPower::POWER_500_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP03=4\r", s.c_str());
}

TEST(Encoder, PARAM_TR_LB)
{
	std::string s;
	ParamValue p = { ParamID::TR_LB, 50U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;ARP06=50\r", s.c_str());
}

TEST(Encoder, PARAM_TR_LB_OutOfRangeCheck)
{
	ParamValue p = { ParamID::TR_LB, 44U};
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::TR_LB, 1201U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_LB_ARGOS_MODE)
{
	std::string s;
	ParamValue p = { ParamID::LB_ARGOS_MODE, BaseArgosMode::OFF};
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP04=0\r", s.c_str());
	p = { ParamID::LB_ARGOS_MODE, BaseArgosMode::LEGACY};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP04=1\r", s.c_str());
	p = { ParamID::LB_ARGOS_MODE, BaseArgosMode::PASS_PREDICTION};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP04=2\r", s.c_str());
	p = { ParamID::LB_ARGOS_MODE, BaseArgosMode::DUTY_CYCLE};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP04=3\r", s.c_str());
}


TEST(Encoder, PARAM_LB_ARGOS_DUTY_CYCLE_OutOfRangeCheck)
{
	ParamValue p = { ParamID::LB_ARGOS_DUTY_CYCLE, 0x1FFFFFFU };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_LB_ARGOS_DUTY_CYCLE)
{
	std::string s;
	ParamValue p = { ParamID::LB_ARGOS_DUTY_CYCLE, 0b101010101010101010101010U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#00E;LBP05=11184810\r", s.c_str());
}

TEST(Encoder, PARAM_LB_GNSS_EN)
{
	std::string s;
	ParamValue p = { ParamID::LB_GNSS_EN, true };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP06=1\r", s.c_str());
	p = { ParamID::LB_GNSS_EN, false };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP06=0\r", s.c_str());
}

TEST(Encoder, PARAM_DLOC_ARG_LB)
{
	std::string s;
	ParamValue p = { ParamID::DLOC_ARG_LB, 10*60U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP12=1\r", s.c_str());
	p = { ParamID::DLOC_ARG_LB, 15*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP12=2\r", s.c_str());
	p = { ParamID::DLOC_ARG_LB, 30*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP12=3\r", s.c_str());
	p = { ParamID::DLOC_ARG_LB, 60*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP12=4\r", s.c_str());
	p = { ParamID::DLOC_ARG_LB, 120*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP12=5\r", s.c_str());
	p = { ParamID::DLOC_ARG_LB, 360*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP12=6\r", s.c_str());
	p = { ParamID::DLOC_ARG_LB, 720*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP12=7\r", s.c_str());
	p = { ParamID::DLOC_ARG_LB, 1440*60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;ARP12=8\r", s.c_str());
}

TEST(Encoder, PARAM_LB_GNSS_HDOPFILT_THR)
{
	std::string s;
	ParamValue p = { ParamID::LB_GNSS_HDOPFILT_THR, 2U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP07=2\r", s.c_str());
}

TEST(Encoder, PARAM_LB_GNSS_HDOPFILT_THR_OutOfRangeCheck)
{
	ParamValue p = { ParamID::LB_GNSS_HDOPFILT_THR, 1U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::LB_GNSS_HDOPFILT_THR, 16U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_LB_ARGOS_DEPTH_PILE)
{
	std::string s;
	ParamValue p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_1 };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP08=1\r", s.c_str());
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_2 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP08=2\r", s.c_str());
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_3 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP08=3\r", s.c_str());
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_4 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP08=4\r", s.c_str());
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_8 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP08=8\r", s.c_str());
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_12 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#007;LBP08=9\r", s.c_str());
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_16 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;LBP08=10\r", s.c_str());
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_20 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;LBP08=11\r", s.c_str());
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_24 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;LBP08=12\r", s.c_str());
}

TEST(Encoder, PARAM_LB_GNSS_ACQ_TIMEOUT)
{
	std::string s;
	ParamValue p = { ParamID::LB_GNSS_ACQ_TIMEOUT, 30U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;LBP09=30\r", s.c_str());
}

TEST(Encoder, PARAM_LB_GNSS_ACQ_TIMEOUT_OutOfRangeCheck)
{
	ParamValue p = { ParamID::LB_GNSS_ACQ_TIMEOUT, 9U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::LB_GNSS_ACQ_TIMEOUT, 61U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_PP_MIN_ELEVATION)
{
	ParamValue p = { ParamID::PP_MIN_ELEVATION, -1.0 };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_MIN_ELEVATION, 91.0};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_MIN_ELEVATION, 45.5};
	v = { p };
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#00A;PPP01=45.5\r", s.c_str());
}

TEST(Encoder, PARAM_PP_MAX_ELEVATION)
{
	ParamValue p = { ParamID::PP_MIN_ELEVATION, -1.0 };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_MAX_ELEVATION, 91.0};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_MAX_ELEVATION, 45.5};
	v = { p };
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#00A;PPP02=45.5\r", s.c_str());
}

TEST(Encoder, PARAM_PP_MIN_DURATION)
{
	ParamValue p = { ParamID::PP_MIN_DURATION, 19U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_MIN_DURATION, 3601U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_MIN_DURATION, 300U};
	v = { p };
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#009;PPP03=300\r", s.c_str());
}

TEST(Encoder, PARAM_PP_MAX_PASSES)
{
	ParamValue p = { ParamID::PP_MAX_PASSES, 0U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_MAX_PASSES, 10001U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_MAX_PASSES, 1000U};
	v = { p };
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#00A;PPP04=1000\r", s.c_str());
}

TEST(Encoder, PARAM_PP_LINEAR_MARGIN)
{
	ParamValue p = { ParamID::PP_LINEAR_MARGIN, 0U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_LINEAR_MARGIN, 3601U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_LINEAR_MARGIN, 300U};
	v = { p };
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#009;PPP05=300\r", s.c_str());
}

TEST(Encoder, PARAM_PP_COMP_STEP)
{
	ParamValue p = { ParamID::PP_COMP_STEP, 0U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_COMP_STEP, 1001U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::PP_COMP_STEP, 30U};
	v = { p };
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	STRCMP_EQUAL("$O;PARMR#008;PPP06=30\r", s.c_str());
}
