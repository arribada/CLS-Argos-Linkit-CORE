#include "dte_protocol.hpp"
#include "debug.hpp"
#include "error.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

using namespace std::literals::string_literals;



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

TEST(Decoder, BooleanParsing)
{
	std::string s;
	s = "$O;PARMR#007;GNP01=1\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_EQUAL(true, (unsigned)std::get<bool>(param_values[0].value));
}

TEST(Decoder, DatestringParsing)
{
	std::string s;
	s = "$O;PARMR#01E;ART01=Tue Feb 03 11:12:44 2009\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_EQUAL(1233659564U, (unsigned)std::get<std::time_t>(param_values[0].value));
}

TEST(Decoder, ArgosPowerParsing)
{
	std::string s;
	s = "$O;PARMR#007;ARP04=4\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosPower::POWER_500_MW == std::get<BaseArgosPower>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP04=3\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosPower::POWER_200_MW == std::get<BaseArgosPower>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP04=2\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosPower::POWER_40_MW == std::get<BaseArgosPower>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP04=1\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosPower::POWER_3_MW == std::get<BaseArgosPower>(param_values[0].value));
	s = "$O;PARMR#009;ARP04=999\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
}

TEST(Decoder, ArgosModeParsing)
{
	std::string s;
	s = "$O;PARMR#007;ARP01=0\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosMode::OFF == std::get<BaseArgosMode>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP01=1\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosMode::PASS_PREDICTION == std::get<BaseArgosMode>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP01=2\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosMode::LEGACY == std::get<BaseArgosMode>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP01=3\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosMode::DUTY_CYCLE == std::get<BaseArgosMode>(param_values[0].value));
	s = "$O;PARMR#007;ARP01=4\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
}


TEST(Decoder, ArgosDepthPile)
{
	std::string s;
	s = "$O;PARMR#007;ARP16=1\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosDepthPile::DEPTH_PILE_1 == std::get<BaseArgosDepthPile>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP16=2\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosDepthPile::DEPTH_PILE_2 == std::get<BaseArgosDepthPile>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP16=3\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosDepthPile::DEPTH_PILE_3 == std::get<BaseArgosDepthPile>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP16=4\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosDepthPile::DEPTH_PILE_4 == std::get<BaseArgosDepthPile>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP16=8\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosDepthPile::DEPTH_PILE_8 == std::get<BaseArgosDepthPile>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP16=9\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosDepthPile::DEPTH_PILE_12 == std::get<BaseArgosDepthPile>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#008;ARP16=10\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosDepthPile::DEPTH_PILE_16 == std::get<BaseArgosDepthPile>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#008;ARP16=11\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosDepthPile::DEPTH_PILE_20 == std::get<BaseArgosDepthPile>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#008;ARP16=12\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(BaseArgosDepthPile::DEPTH_PILE_24 == std::get<BaseArgosDepthPile>(param_values[0].value));
	param_values.clear();
	s = "$O;PARMR#007;ARP16=0\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	s = "$O;PARMR#007;ARP16=5\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	s = "$O;PARMR#007;ARP16=6\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	s = "$O;PARMR#007;ARP16=7\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	s = "$O;PARMR#008;ARP16=13\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
}

TEST(Decoder, MissingLengthSeparator)
{
	std::string s;
	s = "$O;RSTBW000;\r";
	CHECK_FALSE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
}

TEST(Decoder, MissingPayloadSeparator)
{
	std::string s;
	s = "$O;RSTBW#000\r";
	CHECK_FALSE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
}

TEST(Decoder, MissingEndTerminator)
{
	std::string s;
	s = "$N;PARMR#001;3";
	CHECK_FALSE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
}

TEST(Decoder, MissingStartDelimiter)
{
	std::string s;
	s = "N;PARMR#001;3\r";
	CHECK_FALSE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
}

TEST(Decoder, CommandResponseWithErrorCode)
{
	std::string s;
	s = "$N;PARMR#001;3\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_EQUAL(3, error_code);
}

TEST(Decoder, ParamKeyList)
{
	std::string s;
	s = "$PARMR#011;IDT06,IDT07,IDT03\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_EQUAL(0, error_code);
	CHECK_TRUE(DTECommand::PARMR_REQ == command);
	CHECK_TRUE(ParamID::ARGOS_DECID == params[0]);
	CHECK_TRUE(ParamID::ARGOS_HEXID == params[1]);
	CHECK_TRUE(ParamID::FW_APP_VERSION == params[2]);
}

TEST(Decoder, StatKeyList)
{
	std::string s;
	s = "$STATR#011;IDT06,IDT07,IDT03\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_EQUAL(0, error_code);
	CHECK_TRUE(DTECommand::STATR_REQ == command);
	CHECK_TRUE(ParamID::ARGOS_DECID == params[0]);
	CHECK_TRUE(ParamID::ARGOS_HEXID == params[1]);
	CHECK_TRUE(ParamID::FW_APP_VERSION == params[2]);
}

TEST(Decoder, BasicKeyValueList)
{
	std::string s;
	s = "$PARMW#015;IDT06=1234,IDT07=DEAD\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(ParamID::ARGOS_DECID == param_values[0].param);
	CHECK_EQUAL(1234, std::get<unsigned int>(param_values[0].value));
	CHECK_TRUE(ParamID::ARGOS_HEXID == param_values[1].param);
	CHECK_EQUAL(0xDEAD, std::get<unsigned int>(param_values[1].value));
}

TEST(Decoder, MultiTextParams)
{
	std::string s;
	s = "$PARMW#030;IDP11=My profile name,IDP11=Another profile name\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(ParamID::PROFILE_NAME == param_values[0].param);
	CHECK_EQUAL(std::string("My profile name"), std::get<std::string>(param_values[0].value));
	CHECK_TRUE(ParamID::PROFILE_NAME == param_values[1].param);
	CHECK_EQUAL(std::string("Another profile name"), std::get<std::string>(param_values[1].value));
}

TEST(Decoder, KeyValueListWrongArgType)
{
	std::string s;
	s = "$PARMW#015;IDT06=DEAD,IDT07=1234\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
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

TEST(Decoder, ParameterOutOfRange)
{
	std::string s;
	s = "$DUMPM#00A;10000,FFFF\r";
	CHECK_THROWS(ErrorCode, DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::DUMPM_REQ == command);
}

TEST(Decoder, WrongArgType)
{
	std::string s;
	s = "$DUMPM#00A;zzzzz,FFFF\r";
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

TEST(Decoder, EmptyKeyList)
{
	std::string s;
	s = "$PARMR#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_REQ == command);
	s = "$STATR#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::STATR_REQ == command);
}

TEST(Decoder, PARML_REQ)
{
	std::string s;
	s = "$PARML#000;\r";
	CHECK_TRUE(DTECommand::PARML_REQ == command);
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
}

TEST(Decoder, PARMR_REQ)
{
	std::string s;
	s = "$PARMR#00B;IDT06,IDT07\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_REQ == command);
	CHECK_TRUE(ParamID::ARGOS_DECID == params[0]);
	CHECK_TRUE(ParamID::ARGOS_HEXID == params[1]);
}

TEST(Decoder, STATR_REQ)
{
	std::string s;
	s = "$STATR#00B;IDT06,IDT07\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::STATR_REQ == command);
	CHECK_TRUE(ParamID::ARGOS_DECID == params[0]);
	CHECK_TRUE(ParamID::ARGOS_HEXID == params[1]);
}

TEST(Decoder, PARMW_REQ)
{
	std::string s;
	s = "$PARMW#00C;IDT06=123456\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMW_REQ == command);
	CHECK_TRUE(ParamID::ARGOS_DECID == param_values[0].param);
	CHECK_EQUAL(123456, std::get<unsigned int>(param_values[0].value));
}

TEST(Decoder, ZONER_REQ)
{
	std::string s;
	s = "$ZONER#002;01\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::ZONER_REQ == command);
	CHECK_EQUAL(0x01, std::get<unsigned int>(arg_list[0]));
}

TEST(Decoder, ZONEW_REQ)
{
	std::string dummy_zone_file = "Dummy Data For A Zone File";
	std::string s;
	s = "$ZONEW#024;" + websocketpp::base64_encode(dummy_zone_file) + "\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::ZONEW_REQ == command);
	CHECK_EQUAL(dummy_zone_file, std::get<std::string>(arg_list[0]).c_str());
}

#if 0
TEST(Decoder, ZoneDataFromMobileApp)
{
	BaseZone zone;
	std::string zone_data = websocketpp::base64_decode("A/AIQABwbJgqqqqkKDa48a0Vw+g=");
	for (unsigned int i = 0; i < zone_data.size(); i++)
		std::cout << std::setfill('0') << std::setw(2) << std::hex << (((int)zone_data[i]) & 0xFF) << " ";
	std::cout << std::endl;
	ZoneCodec::decode(zone_data, zone);
	std::cout << "zone_id=" << zone.zone_id << std::endl;
	std::cout << "zone_type=" << (int)zone.zone_type << std::endl;
	std::cout << "enable_monitoring=" << zone.enable_monitoring << std::endl;
	std::cout << "enable_entering_leaving_events=" << zone.enable_entering_leaving_events << std::endl;
	std::cout << "enable_out_of_zone_detection_mode=" << zone.enable_out_of_zone_detection_mode << std::endl;
	std::cout << "enable_activation_date=" << zone.enable_activation_date << std::endl;
	std::cout << "year=" << zone.year << std::endl;
	std::cout << "month=" << zone.month << std::endl;
	std::cout << "day=" << zone.day << std::endl;
	std::cout << "hour=" << zone.hour << std::endl;
	std::cout << "minute=" << zone.minute << std::endl;
	std::cout << "comms_vector=" << (int)zone.comms_vector << std::endl;
	std::cout << "delta_arg_loc_argos_seconds=" << zone.delta_arg_loc_argos_seconds << std::endl;
	std::cout << "delta_arg_loc_cellular_seconds=" << zone.delta_arg_loc_cellular_seconds << std::endl;
	std::cout << "argos_extra_flags_enable=" << zone.argos_extra_flags_enable << std::endl;
	std::cout << "argos_depth_pile=" << (int)zone.argos_depth_pile << std::endl;
	std::cout << "argos_power=" << (int)zone.argos_power << std::endl;
	std::cout << "argos_time_repetition_seconds=" << zone.argos_time_repetition_seconds << std::endl;
	std::cout << "argos_mode=" << (int)zone.argos_mode << std::endl;
	std::cout << "argos_duty_cycle=" << zone.argos_duty_cycle << std::endl;
	std::cout << "gnss_extra_flags_enable=" << zone.gnss_extra_flags_enable << std::endl;
	std::cout << "hdop_filter_threshold=" << zone.hdop_filter_threshold << std::endl;
	std::cout << "gnss_acquisition_timeout_seconds=" << zone.gnss_acquisition_timeout_seconds << std::endl;
	std::cout << "center_longitude_x=" << zone.center_longitude_x << std::endl;
	std::cout << "center_latitude_y=" << zone.center_latitude_y << std::endl;
	std::cout << "radius_m=" << zone.radius_m << std::endl;
}
#endif

TEST(Decoder, PROFR_REQ)
{
	std::string s;
	s = "$PROFR#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PROFR_REQ == command);
}

TEST(Decoder, PROFW_REQ)
{
	std::string s;
	s = "$PROFW#01B;Profile Name For Our Device\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PROFW_REQ == command);
	STRCMP_EQUAL("Profile Name For Our Device", std::get<std::string>(arg_list[0]).c_str());
}

TEST(Decoder, PASPW_REQ)
{
	std::string dummy_file = "Dummy Data For A PAPW File";
	std::string s;
	s = "$PASPW#024;" + websocketpp::base64_encode(dummy_file) + "\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PASPW_REQ == command);
	CHECK_EQUAL(dummy_file, std::get<std::string>(arg_list[0]).c_str());
}

TEST(Decoder, SECUR_REQ)
{
	std::string s;
	s = "$SECUR#004;1234\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::SECUR_REQ == command);
	CHECK_EQUAL(0x1234U, std::get<unsigned int>(arg_list[0]));
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

TEST(Decoder, DUMPD_REQ)
{
	std::string s;
	s = "$DUMPD#001;0\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::DUMPD_REQ == command);
	CHECK_EQUAL(0U, std::get<unsigned int>(arg_list[0]));
	s = "$DUMPD#001;1\r";
	arg_list.clear();
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::DUMPD_REQ == command);
	CHECK_EQUAL(1U, std::get<unsigned int>(arg_list[0]));
}

TEST(Decoder, RSTVW_REQ)
{
	std::string s;
	s = "$RSTVW#001;1\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::RSTVW_REQ == command);
	CHECK_EQUAL(1U, std::get<unsigned int>(arg_list[0]));
}

TEST(Decoder, RSTBW_REQ)
{
	std::string s;
	s = "$RSTBW#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::RSTBW_REQ == command);
}

TEST(Decoder, FACTW_REQ)
{
	std::string s;
	s = "$FACTW#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::FACTW_REQ == command);
}

TEST(Decoder, PARML_RESP)
{
	std::string s;
	s = "$O;PARML#00B;IDT06,IDT07\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARML_RESP == command);
	CHECK_TRUE(ParamID::ARGOS_DECID == params[0]);
	CHECK_TRUE(ParamID::ARGOS_HEXID == params[1]);
}

TEST(Decoder, PARMR_RESP)
{
	std::string s;
	s = "$O;PARMR#016;IDT06=57005,IDT07=DEAD\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMR_RESP == command);
	CHECK_TRUE(ParamID::ARGOS_DECID == param_values[0].param);
	CHECK_EQUAL(57005, std::get<unsigned int>(param_values[0].value));
	CHECK_TRUE(ParamID::ARGOS_HEXID == param_values[1].param);
	CHECK_EQUAL(0xDEAD, std::get<unsigned int>(param_values[1].value));
}

TEST(Decoder, STATR_RESP)
{
	std::string s;
	s = "$O;STATR#016;IDT06=57005,IDT07=DEAD\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::STATR_RESP == command);
	CHECK_TRUE(ParamID::ARGOS_DECID == param_values[0].param);
	CHECK_EQUAL(57005, std::get<unsigned int>(param_values[0].value));
	CHECK_TRUE(ParamID::ARGOS_HEXID == param_values[1].param);
	CHECK_EQUAL(0xDEAD, std::get<unsigned int>(param_values[1].value));
}

TEST(Decoder, PARMW_RESP)
{
	std::string s;
	s = "$O;PARMW#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PARMW_RESP == command);
}

TEST(Decoder, ZONER_RESP)
{
	std::string dummy_file = "Dummy Data For A Zone File";
	std::string s;
	s = "$O;ZONER#024;" + websocketpp::base64_encode(dummy_file) + "\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::ZONER_RESP == command);
	CHECK_EQUAL(dummy_file, std::get<std::string>(arg_list[0]).c_str());
}

TEST(Decoder, ZONEW_RESP)
{
	std::string s;
	s = "$O;ZONEW#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::ZONEW_RESP == command);
}

TEST(Decoder, PROFR_RESP)
{
	std::string s;
	s = "$O;PROFR#01B;Profile Name For Our Device\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PROFR_RESP == command);
	STRCMP_EQUAL("Profile Name For Our Device", std::get<std::string>(arg_list[0]).c_str());
}

TEST(Decoder, PROFW_RESP)
{
	std::string s;
	s = "$O;PROFW#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PROFW_RESP == command);
}

TEST(Decoder, PASPW_RESP)
{
	std::string s;
	s = "$O;PASPW#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::PASPW_RESP == command);
}

TEST(Decoder, SECUR_RESP)
{
	std::string s;
	s = "$O;SECUR#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::SECUR_RESP == command);
}

TEST(Decoder, DUMPM_RESP)
{
	std::string dummy_file = "Dummy Data For A Dump File";
	std::string s;
	s = "$O;DUMPM#024;" + websocketpp::base64_encode(dummy_file) + "\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::DUMPM_RESP == command);
	CHECK_EQUAL(dummy_file, std::get<std::string>(arg_list[0]).c_str());
}

TEST(Decoder, DUMPD_RESP)
{
	std::string dummy_file = "Dummy Data For A Dump File";
	std::string s;
	s = "$O;DUMPD#028;0,1," + websocketpp::base64_encode(dummy_file) + "\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::DUMPD_RESP == command);
	CHECK_EQUAL(0U, std::get<unsigned int>(arg_list[0]));
	CHECK_EQUAL(1U, std::get<unsigned int>(arg_list[1]));
	CHECK_EQUAL(dummy_file, std::get<std::string>(arg_list[2]).c_str());
}

TEST(Decoder, RSTVW_RESP)
{
	std::string s;
	s = "$O;RSTVW#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::RSTVW_RESP == command);
}

TEST(Decoder, RSTBW_RESP)
{
	std::string s;
	s = "$O;RSTBW#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::RSTBW_RESP == command);
}

TEST(Decoder, FACTW_RESP)
{
	std::string s;
	s = "$O;FACTW#000;\r";
	CHECK_TRUE(DTEDecoder::decode(s, command, error_code, arg_list, params, param_values));
	CHECK_TRUE(DTECommand::FACTW_RESP == command);
}
