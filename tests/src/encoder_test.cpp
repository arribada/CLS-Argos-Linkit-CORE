#include "dte_protocol.hpp"
#include "debug.hpp"
#include "error.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


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
	CHECK_EQUAL("$PARML#000;\r", s);
}

TEST(Encoder, PARML_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARML_RESP, params);
	CHECK_EQUAL("$O;PARML#04D;IDT07,IDT06,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP09,ART03,ARP03,ARP04,ARP05\r", s);
}

TEST(Encoder, PARMR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMR_REQ, params);
	CHECK_EQUAL("$PARMR#04D;IDT07,IDT06,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP09,ART03,ARP03,ARP04,ARP05\r", s);
}

TEST(Encoder, PARMW_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PARMW_REQ, values);
	CHECK_EQUAL("$PARMW#016;IDT07=DEAD,IDT06=57005\r", s);
}

TEST(Encoder, ZONEW_REQ)
{
	std::string s;
	// Convert a buffer to base64
	unsigned char buffer[] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer) };
	s = DTEEncoder::encode(DTECommand::ZONEW_REQ, raw_data);
	CHECK_EQUAL("$ZONEW#014;AAECAwQFBgcJCgsMDQ4P\r", s);
}

TEST(Encoder, PROFR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFR_REQ);
	CHECK_EQUAL("$PROFR#000;\r", s);
}

TEST(Encoder, PROFR_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFR_RESP, 0, std::string("Underwater Vehicle"));
	CHECK_EQUAL("$O;PROFR#012;Underwater Vehicle\r", s);
}

TEST(Encoder, PROFW_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFW_REQ, std::string("Underwater Vehicle"));
	CHECK_EQUAL("$PROFW#012;Underwater Vehicle\r", s);
}

TEST(Encoder, PROFW_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PROFW_RESP, 0);
	CHECK_EQUAL("$O;PROFW#000;\r", s);
}

TEST(Encoder, PASPW_REQ)
{
	std::string s;
	unsigned char buffer[] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer) };
	s = DTEEncoder::encode(DTECommand::PASPW_REQ, raw_data);
	CHECK_EQUAL("$PASPW#014;AAECAwQFBgcJCgsMDQ4P\r", s);
}

TEST(Encoder, PASPW_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::PASPW_RESP, 0);
	CHECK_EQUAL("$O;PASPW#000;\r", s);
}

TEST(Encoder, SECUR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::SECUR_REQ);
	CHECK_EQUAL("$SECUR#000;\r", s);
}

TEST(Encoder, SECUR_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::SECUR_RESP, 0);
	CHECK_EQUAL("$O;SECUR#000;\r", s);
}

TEST(Encoder, DUMPM_REQ)
{
	std::string s;
	// Command with multi-args
	s = DTEEncoder::encode(DTECommand::DUMPM_REQ, 0x10000, 0x200);
	CHECK_EQUAL("$DUMPM#009;10000,200\r", s);

	// Surplus args are ignored
	s = DTEEncoder::encode(DTECommand::DUMPM_REQ, 0x10000, 0x200, 1, 2, 3);
	CHECK_EQUAL("$DUMPM#009;10000,200\r", s);
}

TEST(Encoder, DUMPM_RESP)
{
	std::string s;
	unsigned char buffer[0x200] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer) };
	s = DTEEncoder::encode(DTECommand::DUMPM_RESP, 0, raw_data);
	CHECK_EQUAL("$O;DUMPM#2AC;AAECAwQFBgcJCgsMDQ4PAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\r", s);
}

TEST(Encoder, DUMPM_RESP_ERROR)
{
	std::string s;
	// Command response with an error code
	s = DTEEncoder::encode(DTECommand::DUMPM_RESP, 3);
	CHECK_EQUAL("$N;DUMPM#001;3\r", s);
}

TEST(Encoder, DUMPL_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::DUMPL_REQ);
	CHECK_EQUAL("$DUMPL#000;\r", s);
}

TEST(Encoder, DUMPL_RESP)
{
	std::string s;
	unsigned char buffer[0x200] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer) };
	s = DTEEncoder::encode(DTECommand::DUMPL_RESP, 0, raw_data);
	CHECK_EQUAL("$O;DUMPL#2AC;AAECAwQFBgcJCgsMDQ4PAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\r", s);
}

TEST(Encoder, DUMPD_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::DUMPD_REQ);
	CHECK_EQUAL("$DUMPD#000;\r", s);
}

TEST(Encoder, DUMPD_RESP)
{
	std::string s;
	unsigned char buffer[0x200] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer) };
	s = DTEEncoder::encode(DTECommand::DUMPD_RESP, 0, raw_data);
	CHECK_EQUAL("$O;DUMPD#2AC;AAECAwQFBgcJCgsMDQ4PAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\r", s);
}

TEST(Encoder, FACTR_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::FACTR_REQ);
	CHECK_EQUAL("$FACTR#000;\r", s);
}

TEST(Encoder, FACTR_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::FACTR_RESP, 0);
	CHECK_EQUAL("$O;FACTR#000;\r", s);
}

TEST(Encoder, RESET_REQ)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::RESET_REQ);
	CHECK_EQUAL("$RESET#000;\r", s);
}

TEST(Encoder, RESET_RESP)
{
	std::string s;
	s = DTEEncoder::encode(DTECommand::RESET_RESP, 0);
	CHECK_EQUAL("$O;RESET#000;\r", s);
}

TEST(Encoder, PARAM_ARGOS_DECID_RANGE)
{
	ParamValue p = { ParamID::ARGOS_DECID, 0xFFFFFFFFU };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(Encoder, PARAM_ARGOS_HEXID_RANGE)
{
	ParamValue p = { ParamID::ARGOS_HEXID, 0xFFFFFFFFU };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(Encoder, PARAM_ARGOS_DEVICE_MODEL_RANGE)
{
	ParamValue p = { ParamID::DEVICE_MODEL, 0x100U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(Encoder, PARAM_ARGOS_FW_APP_VERSION)
{
	std::string s;
	ParamValue p = { ParamID::FW_APP_VERSION, "V1.2.3.4" };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#00E;IDT03=V1.2.3.4\r", s);
}

TEST(Encoder, PARAM_ARGOS_FW_APP_VERSION_RANGE)
{
	ParamValue p = { ParamID::FW_APP_VERSION, "V1.2.3.4xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMW_REQ, v));
}

TEST(Encoder, PARAM_LAST_TX)
{
	std::string s;
	ParamValue p = { ParamID::LAST_TX, (std::time_t)0 };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#01E;ART01=Thu Jan  1 00:00:00 1970\r", s);
}

TEST(Encoder, PARAM_TX_COUNTER)
{
	std::string s;
	ParamValue p = { ParamID::TX_COUNTER, 22U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ART02=22\r", s);
}

TEST(Encoder, PARAM_BATT_SOC_RANGE)
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
	CHECK_EQUAL("$O;PARMR#009;POT03=100\r", s);

	p = { ParamID::BATT_SOC, 0U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;POT03=0\r", s);
}

TEST(Encoder, PARAM_LAST_FULL_CHARGE_DATE)
{
	std::string s;
	ParamValue p = { ParamID::LAST_FULL_CHARGE_DATE, (std::time_t)0 };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#01E;POT05=Thu Jan  1 00:00:00 1970\r", s);
}

TEST(Encoder, PARAM_ARGOS_PROFILE_NAME)
{
	std::string s;
	ParamValue p = { ParamID::PROFILE_NAME, "Turtle Tracker" };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#014;IDP09=Turtle Tracker\r", s);
}

TEST(Encoder, PARAM_ARGOS_AOP_STATUS)
{
	std::string s;
	unsigned char buffer[] = { 0,1,2,3,4,5,6,7,9,10,11,12,13,14,15 };
	BaseRawData raw_data = { buffer, sizeof(buffer) };
	ParamValue p = { ParamID::AOP_STATUS, raw_data };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#01A;XXXXX=AAECAwQFBgcJCgsMDQ4P\r", s);
}

TEST(Encoder, PARAM_ARGOS_AOP_DATE)
{
	std::string s;
	ParamValue p = { ParamID::ARGOS_AOP_DATE, (std::time_t)0 };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#01E;ART03=Thu Jan  1 00:00:00 1970\r", s);
}

TEST(Encoder, PARAM_ARGOS_FREQ)
{
	std::string s;
	ParamValue p = { ParamID::ARGOS_FREQ, 400.1};
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#00B;ARP03=400.1\r", s);
}

TEST(Encoder, PARAM_ARGOS_FREQ_RANGE)
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
	ParamValue p = { ParamID::ARGOS_POWER, BaseArgosPower::POWER_250_MW};
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;ARP04=250\r", s);
	p = { ParamID::ARGOS_POWER, BaseArgosPower::POWER_500_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;ARP04=500\r", s);
	p = { ParamID::ARGOS_POWER, BaseArgosPower::POWER_750_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;ARP04=750\r", s);
	p = { ParamID::ARGOS_POWER, BaseArgosPower::POWER_1000_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#00A;ARP04=1000\r", s);
}

TEST(Encoder, PARAM_TR_NOM)
{
	std::string s;
	ParamValue p = { ParamID::TR_NOM, 50U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP05=50\r", s);
}

TEST(Encoder, PARAM_TR_NOM_RANGE)
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
	CHECK_EQUAL("$O;PARMR#007;ARP01=0\r", s);
	p = { ParamID::ARGOS_MODE, BaseArgosMode::LEGACY};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;ARP01=1\r", s);
	p = { ParamID::ARGOS_MODE, BaseArgosMode::PASS_PREDICTION};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;ARP01=2\r", s);
	p = { ParamID::ARGOS_MODE, BaseArgosMode::DUTY_CYCLE};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;ARP01=3\r", s);
}


TEST(Encoder, PARAM_NTRY_PER_MESSAGE)
{
	std::string s;
	ParamValue p = { ParamID::NTRY_PER_MESSAGE, 3U};
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;ARP19=3\r", s);
}

TEST(Encoder, PARAM_NTRY_PER_MESSAGE_RANGE)
{
	ParamValue p = { ParamID::NTRY_PER_MESSAGE, 86401U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_DUTY_CYCLE_RANGE)
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
	CHECK_EQUAL("$O;PARMR#00C;ARP18=AAAAAA\r", s);
}

TEST(Encoder, PARAM_GNSS_EN)
{
	std::string s;
	ParamValue p = { ParamID::GNSS_EN, true };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;GNP01=1\r", s);
	p = { ParamID::GNSS_EN, false };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;GNP01=0\r", s);
}

TEST(Encoder, PARAM_DLOC_ARG_NOM)
{
	std::string s;
	ParamValue p = { ParamID::DLOC_ARG_NOM, 10U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP11=10\r", s);
	p = { ParamID::DLOC_ARG_NOM, 15U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP11=15\r", s);
	p = { ParamID::DLOC_ARG_NOM, 30U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP11=30\r", s);
	p = { ParamID::DLOC_ARG_NOM, 60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP11=60\r", s);
	p = { ParamID::DLOC_ARG_NOM, 120U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;ARP11=120\r", s);
	p = { ParamID::DLOC_ARG_NOM, 360U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;ARP11=360\r", s);
	p = { ParamID::DLOC_ARG_NOM, 720U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;ARP11=720\r", s);
	p = { ParamID::DLOC_ARG_NOM, 1440U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#00A;ARP11=1440\r", s);
}

TEST(Encoder, PARAM_DLOC_ARG_NOM_RANGE)
{
	ParamValue p = { ParamID::DLOC_ARG_NOM, 22U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}

TEST(Encoder, PARAM_ARGOS_DEPTH_PILE)
{
	std::string s;
	ParamValue p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_1 };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;ARP16=1\r", s);
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_2 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;ARP16=2\r", s);
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_3 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;ARP16=3\r", s);
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_4 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;ARP16=4\r", s);
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_8 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;ARP16=8\r", s);
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_12 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;ARP16=9\r", s);
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_16 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP16=10\r", s);
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_20 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP16=11\r", s);
	p = { ParamID::ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_24 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP16=12\r", s);
}

TEST(Encoder, PARAM_GNSS_HDOPFILT_EN)
{
	std::string s;
	ParamValue p = { ParamID::GNSS_HDOPFILT_EN, true };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;GNP02=1\r", s);
	p = { ParamID::GNSS_HDOPFILT_EN, false };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;GNP02=0\r", s);
}

TEST(Encoder, PARAM_GNSS_HDOPFILT_THR_)
{
	std::string s;
	ParamValue p = { ParamID::GNSS_HDOPFILT_THR, 2U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;GNP03=2\r", s);
}

TEST(Encoder, PARAM_GNSS_HDOPFILT_THR_RANGE)
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
	CHECK_EQUAL("$O;PARMR#008;GNP05=30\r", s);
}

TEST(Encoder, PARAM_GNSS_ACQ_TIMEOUT_RANGE)
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
	CHECK_EQUAL("$O;PARMR#007;UNP01=1\r", s);
	p = { ParamID::UNDERWATER_EN, false };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;UNP01=0\r", s);
}

TEST(Encoder, PARAM_DRY_TIME_BEFORE_TX)
{
	std::string s;
	ParamValue p = { ParamID::DRY_TIME_BEFORE_TX, 10U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;UNP02=10\r", s);
}

TEST(Encoder, PARAM_DRY_TIME_BEFORE_TX_RANGE)
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
	CHECK_EQUAL("$O;PARMR#008;UNP03=10\r", s);
}

TEST(Encoder, PARAM_SAMPLING_UNDER_FREQ_RANGE)
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
	CHECK_EQUAL("$O;PARMR#007;LBP01=1\r", s);
	p = { ParamID::LB_EN, false };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP01=0\r", s);
}

TEST(Encoder, PARAM_LB_TRESHOLD)
{
	std::string s;
	ParamValue p = { ParamID::LB_TRESHOLD, 10U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;LBP02=10\r", s);
}

TEST(Encoder, PARAM_LB_TRESHOLD_RANGE)
{
	ParamValue p = { ParamID::LB_TRESHOLD, 101U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}


TEST(Encoder, PARAM_LB_ARGOS_POWER)
{
	std::string s;
	ParamValue p = { ParamID::LB_ARGOS_POWER, BaseArgosPower::POWER_250_MW};
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;LBP03=250\r", s);
	p = { ParamID::LB_ARGOS_POWER, BaseArgosPower::POWER_500_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;LBP03=500\r", s);
	p = { ParamID::LB_ARGOS_POWER, BaseArgosPower::POWER_750_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;LBP03=750\r", s);
	p = { ParamID::LB_ARGOS_POWER, BaseArgosPower::POWER_1000_MW};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#00A;LBP03=1000\r", s);
}

TEST(Encoder, PARAM_TR_LB)
{
	std::string s;
	ParamValue p = { ParamID::TR_LB, 50U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP06=50\r", s);
}

TEST(Encoder, PARAM_TR_LB_RANGE)
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
	CHECK_EQUAL("$O;PARMR#007;LBP04=0\r", s);
	p = { ParamID::LB_ARGOS_MODE, BaseArgosMode::LEGACY};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP04=1\r", s);
	p = { ParamID::LB_ARGOS_MODE, BaseArgosMode::PASS_PREDICTION};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP04=2\r", s);
	p = { ParamID::LB_ARGOS_MODE, BaseArgosMode::DUTY_CYCLE};
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP04=3\r", s);
}


TEST(Encoder, PARAM_LB_ARGOS_DUTY_CYCLE_RANGE)
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
	CHECK_EQUAL("$O;PARMR#00C;LBP05=AAAAAA\r", s);
}

TEST(Encoder, PARAM_LB_GNSS_EN)
{
	std::string s;
	ParamValue p = { ParamID::LB_GNSS_EN, true };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP06=1\r", s);
	p = { ParamID::LB_GNSS_EN, false };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP06=0\r", s);
}

TEST(Encoder, PARAM_DLOC_ARG_LB)
{
	std::string s;
	ParamValue p = { ParamID::DLOC_ARG_LB, 10U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	p = { ParamID::DLOC_ARG_LB, 15U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP12=15\r", s);
	p = { ParamID::DLOC_ARG_LB, 30U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP12=30\r", s);
	p = { ParamID::DLOC_ARG_LB, 60U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;ARP12=60\r", s);
	p = { ParamID::DLOC_ARG_LB, 120U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;ARP12=120\r", s);
	p = { ParamID::DLOC_ARG_LB, 360U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;ARP12=360\r", s);
	p = { ParamID::DLOC_ARG_LB, 720U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#009;ARP12=720\r", s);
	p = { ParamID::DLOC_ARG_LB, 1440U };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#00A;ARP12=1440\r", s);
}

TEST(Encoder, PARAM_LB_GNSS_HDOPFILT_THR)
{
	std::string s;
	ParamValue p = { ParamID::LB_GNSS_HDOPFILT_THR, 2U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP07=2\r", s);
}

TEST(Encoder, PARAM_LB_GNSS_HDOPFILT_THR_RANGE)
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
	CHECK_EQUAL("$O;PARMR#007;LBP08=1\r", s);
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_2 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP08=2\r", s);
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_3 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP08=3\r", s);
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_4 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP08=4\r", s);
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_8 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP08=8\r", s);
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_12 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#007;LBP08=9\r", s);
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_16 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;LBP08=10\r", s);
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_20 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;LBP08=11\r", s);
	p = { ParamID::LB_ARGOS_DEPTH_PILE, BaseArgosDepthPile::DEPTH_PILE_24 };
	v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;LBP08=12\r", s);
}

TEST(Encoder, PARAM_LB_GNSS_ACQ_TIMEOUT)
{
	std::string s;
	ParamValue p = { ParamID::LB_GNSS_ACQ_TIMEOUT, 30U };
	std::vector<ParamValue> v = { p };
	s = DTEEncoder::encode(DTECommand::PARMR_RESP, v);
	CHECK_EQUAL("$O;PARMR#008;LBP09=30\r", s);
}

TEST(Encoder, PARAM_LB_GNSS_ACQ_TIMEOUT_RANGE)
{
	ParamValue p = { ParamID::LB_GNSS_ACQ_TIMEOUT, 9U };
	std::vector<ParamValue> v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
	p = { ParamID::LB_GNSS_ACQ_TIMEOUT, 61U};
	v = { p };
	CHECK_THROWS(ErrorCode, DTEEncoder::encode(DTECommand::PARMR_RESP, v));
}
