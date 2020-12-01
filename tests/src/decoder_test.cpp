#include "dte_protocol.hpp"
#include "debug.hpp"
#include "error.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


TEST_GROUP(Decoder)
{
	DTECommand command;
	std::vector<ParamID> params;
	std::vector<ParamValue> param_values;
	std::vector<BaseType> arg_list;
	unsigned int error_code;
	void setup() {
		error_code = 0;
	}
	void teardown() {
	}
};

TEST(Decoder, KeyList)
{
	std::string s;
	s = "$PARMR#011;IDT06,IDT07,IDT03\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_REQ == command);
	CHECK_TRUE(ParamID::ARGOS_DECID == params[0]);
	CHECK_TRUE(ParamID::ARGOS_HEXID == params[1]);
	CHECK_TRUE(ParamID::FW_APP_VERSION == params[2]);
}

TEST(Decoder, KeyValueList)
{
}

TEST(Decoder, Sanity)
{
	std::string s;

	s = "$O;RESET#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::RESET_RESP == command);

	s = "$N;RESET#001;3\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::RESET_RESP == command);
	CHECK_EQUAL(3, error_code);

	s = "$PARML#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARML_REQ == command);

}

TEST(Decoder, UnknownCommand)
{
	std::string s;
	s = "$XXXXX#009;10000,200\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
}

TEST(Decoder, PayloadLengthMismatch)
{
	std::string s;
	s = "$DUMPM#00A;10000,200\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
}

TEST(Decoder, DUMPM_REQ)
{
	std::string s;
	s = "$DUMPM#009;10000,200\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::DUMPM_REQ == command);
	CHECK_EQUAL(0x10000U, std::get<unsigned int>(arg_list[0]));
	CHECK_EQUAL(0x200U, std::get<unsigned int>(arg_list[1]));
}

TEST(Decoder, DUMPM_REQ_ParameterOutOfRange)
{
	std::string s;
	s = "$DUMPM#00A;10000,FFFF\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::DUMPM_REQ == command);
}

TEST(Decoder, IncorrectParameterSeparator)
{
	std::string s;
	s = "$DUMPM#009;10000^FFFF\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::DUMPM_REQ == command);
}

TEST(Decoder, UnexpectedArgument)
{
	std::string s;
	s = "$PARML#005;10000\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARML_REQ == command);
}

TEST(Decoder, MissingArgument)
{
	std::string s;
	s = "$DUMPM#005;10000\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::DUMPM_REQ == command);
}

TEST(Decoder, NoArgumentSupplied)
{
	std::string s;
	s = "$DUMPM#000;\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::DUMPM_REQ == command);
}
