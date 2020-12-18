#include "dte_handler.hpp"
#include "config_store_fs.hpp"
#include "fake_memory_access.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "mock_logger.hpp"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)


extern FileSystem *main_filesystem;
extern ConfigurationStore *configuration_store;
extern MemoryAccess *memory_access;
extern Logger *sensor_log;
extern Logger *system_log;


TEST_GROUP(DTEHandler)
{
	LFSRamFileSystem *ram_filesystem;
	LFSConfigurationStore *store;
	FakeMemoryAccess *fake_memory_access;
	MockLog *mock_system_log;
	MockLog *mock_sensor_log;

	void setup() {
		ram_filesystem = new LFSRamFileSystem(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
		ram_filesystem->format();
		ram_filesystem->mount();
		main_filesystem = ram_filesystem;
		store = new LFSConfigurationStore(*ram_filesystem);
		store->init();
		configuration_store = store;
		fake_memory_access = new FakeMemoryAccess();
		memory_access = fake_memory_access;
		mock_system_log = new MockLog;
		system_log = mock_system_log;
		mock_sensor_log = new MockLog;
		sensor_log = mock_sensor_log;
	}

	void teardown() {
		delete mock_sensor_log;
		delete mock_system_log;
		delete fake_memory_access;
		delete store;
		ram_filesystem->umount();
		delete ram_filesystem;
	}
};


TEST(DTEHandler, PARML_REQ)
{
	std::string resp;
	std::string req = DTEEncoder::encode(DTECommand::PARML_REQ);
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PARML#0DD;IDT06,IDT07,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP11,ART03,ARP03,ARP04,ARP05,ARP01,ARP19,ARP18,GNP01,ARP11,ARP16,GNP02,GNP03,GNP05,UNP01,UNP02,UNP03,LBP01,LBP02,LBP03,ARP06,LBP04,LBP05,LBP06,ARP12,LBP07,LBP08,LBP09,UNP04\r", resp.c_str());
}

TEST(DTEHandler, PARMW_REQ)
{
	std::string resp;
	std::string req = "$PARMW#007;ARP04=4\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PARMW#000;\r", resp.c_str());
	CHECK_TRUE(BaseArgosPower::POWER_500_MW == configuration_store->read_param<BaseArgosPower>(ParamID::ARGOS_POWER));
}

TEST(DTEHandler, PARMR_REQ)
{
	std::string resp;
	std::string req = "$PARMR#0D7;IDT06,IDT07,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP11,ART03,ARP03,ARP04,ARP05,ARP01,ARP19,ARP18,GNP01,ARP11,ARP16,GNP02,GNP03,GNP05,UNP01,UNP02,UNP03,LBP01,LBP02,LBP03,ARP06,LBP04,LBP05,LBP06,ARP12,LBP07,LBP08,LBP09\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PARMR#178;IDT06=0,IDT07=0,IDT02=GenTracker,IDT03=V0.1,ART01=Thu Jan  1 00:00:00 1970,ART02=0,POT03=0,POT05=Thu Jan  1 00:00:00 1970,IDP11=,ART03=Thu Jan  1 00:00:00 1970,ARP03=399.91,ARP04=4,ARP05=45,ARP01=0,ARP19=1,ARP18=0,GNP01=0,ARP11=1,ARP16=1,GNP02=0,GNP03=2,GNP05=60,UNP01=0,UNP02=1,UNP03=1,LBP01=0,LBP02=0,LBP03=4,ARP06=45,LBP04=0,LBP05=0,LBP06=0,ARP12=4,LBP07=2,LBP08=1,LBP09=60\r", resp.c_str());
}

TEST(DTEHandler, STATR_REQ)
{
	std::string resp;
	std::string req = "$STATR#0D7;IDT06,IDT07,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP11,ART03,ARP03,ARP04,ARP05,ARP01,ARP19,ARP18,GNP01,ARP11,ARP16,GNP02,GNP03,GNP05,UNP01,UNP02,UNP03,LBP01,LBP02,LBP03,ARP06,LBP04,LBP05,LBP06,ARP12,LBP07,LBP08,LBP09\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;STATR#178;IDT06=0,IDT07=0,IDT02=GenTracker,IDT03=V0.1,ART01=Thu Jan  1 00:00:00 1970,ART02=0,POT03=0,POT05=Thu Jan  1 00:00:00 1970,IDP11=,ART03=Thu Jan  1 00:00:00 1970,ARP03=399.91,ARP04=4,ARP05=45,ARP01=0,ARP19=1,ARP18=0,GNP01=0,ARP11=1,ARP16=1,GNP02=0,GNP03=2,GNP05=60,UNP01=0,UNP02=1,UNP03=1,LBP01=0,LBP02=0,LBP03=4,ARP06=45,LBP04=0,LBP05=0,LBP06=0,ARP12=4,LBP07=2,LBP08=1,LBP09=60\r", resp.c_str());
}

TEST(DTEHandler, STATR_REQ_CheckEmptyRequest)
{
	std::string resp;
	std::string req = "$STATR#000;\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;STATR#098;IDT06=0,IDT07=0,IDT02=GenTracker,IDT03=V0.1,ART01=Thu Jan  1 00:00:00 1970,ART02=0,POT03=0,POT05=Thu Jan  1 00:00:00 1970,ART03=Thu Jan  1 00:00:00 1970\r", resp.c_str());
}

TEST(DTEHandler, PARMR_REQ_CheckEmptyRequest)
{
	std::string resp;
	std::string req = "$PARMR#000;\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PARMR#0E7;IDP11=,ARP03=399.91,ARP04=4,ARP05=45,ARP01=0,ARP19=1,ARP18=0,GNP01=0,ARP11=1,ARP16=1,GNP02=0,GNP03=2,GNP05=60,UNP01=0,UNP02=1,UNP03=1,LBP01=0,LBP02=0,LBP03=4,ARP06=45,LBP04=0,LBP05=0,LBP06=0,ARP12=4,LBP07=2,LBP08=1,LBP09=60,UNP04=1\r", resp.c_str());
}

TEST(DTEHandler, PROFW_PROFR_REQ)
{
	std::string resp;
	std::string req = "$PROFW#018;Profile Name For Tracker\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PROFW#000;\r", resp.c_str());
	req = "$PROFR#000;\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PROFR#018;Profile Name For Tracker\r", resp.c_str());
}

TEST(DTEHandler, SECUR_REQ)
{
	std::string resp;
	std::string req = "$SECUR#000;\r";
	CHECK_TRUE(DTEAction::SECUR == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;SECUR#000;\r", resp.c_str());
}

TEST(DTEHandler, RSTBW_REQ)
{
	std::string resp;
	std::string req = "$RSTBW#000;\r";
	CHECK_TRUE(DTEAction::RESET == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;RSTBW#000;\r", resp.c_str());
}

TEST(DTEHandler, FACTW_REQ)
{
	std::string resp;
	std::string req = "$FACTW#000;\r";
	CHECK_TRUE(DTEAction::FACTR == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;FACTW#000;\r", resp.c_str());
}

TEST(DTEHandler, DUMPM_REQ)
{
	std::string resp;
	std::string req = "$DUMPM#007;100,200\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;DUMPM#2AC;AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/wABAgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICEiIyQlJicoKSorLC0uLzAxMjM0NTY3ODk6Ozw9Pj9AQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVpbXF1eX2BhYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ent8fX5/gIGCg4SFhoeIiYqLjI2Oj5CRkpOUlZaXmJmam5ydnp+goaKjpKWmp6ipqqusra6vsLGys7S1tre4ubq7vL2+v8DBwsPExcbHyMnKy8zNzs/Q0dLT1NXW19jZ2tvc3d7f4OHi4+Tl5ufo6err7O3u7/Dx8vP09fb3+Pn6+/z9/v8=\r", resp.c_str());
}

TEST(DTEHandler, ZONEW_REQ)
{
	BaseRawData zone_raw = {0,0, ""};
	BaseZone zone;
	zone.zone_id = 1;
	zone.zone_type = BaseZoneType::CIRCLE;
	zone.enable_monitoring = true;
	zone.enable_entering_leaving_events = true;
	zone.enable_out_of_zone_detection_mode = true;
	zone.enable_activation_date = true;
	zone.year = 1970;
	zone.month = 1;
	zone.day = 1;
	zone.hour = 0;
	zone.minute = 0;
	zone.comms_vector = BaseCommsVector::ARGOS_PREFERRED;
	zone.delta_arg_loc_argos_seconds = 7*60U;
	zone.delta_arg_loc_cellular_seconds = 0;
	zone.argos_extra_flags_enable = true;
	zone.argos_depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	zone.argos_power = BaseArgosPower::POWER_500_MW;
	zone.argos_time_repetition_seconds = 60U;
	zone.argos_mode = BaseArgosMode::DUTY_CYCLE;
	zone.argos_duty_cycle = 0b101010101010101010101010;
	zone.gnss_extra_flags_enable = true;
	zone.hdop_filter_threshold = 2U;
	zone.gnss_acquisition_timeout_seconds = 60U;
	zone.center_latitude_y = 0;
	zone.center_longitude_x = 0;
	zone.radius_m = 0;
	ZoneCodec::encode(zone, zone_raw.str);

	std::string resp;
	std::string req = DTEEncoder::encode(DTECommand::ZONEW_REQ, zone_raw);
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;ZONEW#000;\r", resp.c_str());

	BaseZone& stored_zone = configuration_store->read_zone();
	CHECK_TRUE(stored_zone == zone);
}

TEST(DTEHandler, ZONER_REQ)
{
	BaseZone zone;
	zone.zone_id = 1;
	zone.zone_type = BaseZoneType::CIRCLE;
	zone.enable_monitoring = true;
	zone.enable_entering_leaving_events = true;
	zone.enable_out_of_zone_detection_mode = true;
	zone.enable_activation_date = true;
	zone.year = 1970;
	zone.month = 1;
	zone.day = 1;
	zone.hour = 0;
	zone.minute = 0;
	zone.comms_vector = BaseCommsVector::ARGOS_PREFERRED;
	zone.delta_arg_loc_argos_seconds = 7*60U;
	zone.delta_arg_loc_cellular_seconds = 0;
	zone.argos_extra_flags_enable = true;
	zone.argos_depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	zone.argos_power = BaseArgosPower::POWER_500_MW;
	zone.argos_time_repetition_seconds = 60U;
	zone.argos_mode = BaseArgosMode::DUTY_CYCLE;
	zone.argos_duty_cycle = 0b101010101010101010101010;
	zone.gnss_extra_flags_enable = true;
	zone.hdop_filter_threshold = 2U;
	zone.gnss_acquisition_timeout_seconds = 60U;
	zone.center_latitude_y = 0;
	zone.center_longitude_x = 0;
	zone.radius_m = 0;
	configuration_store->write_zone(zone);

	std::string resp;
	std::string req = DTEEncoder::encode(DTECommand::ZONER_REQ, 1);
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	CHECK_EQUAL("$O;ZONER#01C;ge8iACAKjA2rqqoWHgAAAAAAAAA=\r"s, resp);

	std::string zone_resp_bits = websocketpp::base64_decode("ge8iACAKjA2rqqoWHgAAAAAAAAA="s);
	BaseZone zone_resp_decoded;
	ZoneCodec::decode(zone_resp_bits, zone_resp_decoded);

	CHECK_TRUE(zone == zone_resp_decoded);
}

TEST(DTEHandler, PASPW_REQ)
{
	BaseRawData paspw_raw = {0,0, ""};
	BasePassPredict pass_predict;
	pass_predict.num_records = 1;
	PassPredictCodec::encode(pass_predict, paspw_raw.str);

	std::string resp;
	std::string req = DTEEncoder::encode(DTECommand::PASPW_REQ, paspw_raw);
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PASPW#000;\r", resp.c_str());

	BasePassPredict& stored_pass_predict = configuration_store->read_pass_predict();
	CHECK_TRUE(stored_pass_predict == pass_predict);
}

TEST(DTEHandler, DUMPL_REQ)
{
	std::string req = DTEEncoder::encode(DTECommand::DUMPL_REQ);
	std::string resp;

	mock().expectOneCall("num_entries").onObject(mock_system_log).andReturnValue(1);
	mock().expectOneCall("read").onObject(mock_system_log).withIntParameter("index", 0).ignoreOtherParameters();

	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;DUMPL#0AC;AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\r", resp.c_str());

	// Check last 16 are retrieved when more than 16 entries are in the log file
	mock().expectOneCall("num_entries").onObject(mock_system_log).andReturnValue(20);
	for (unsigned int i = 0; i < 16; i++)
		mock().expectOneCall("read").onObject(mock_system_log).withIntParameter("index", i+4).ignoreOtherParameters();
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));

	mock().checkExpectations();
}

TEST(DTEHandler, DUMPD_REQ)
{
	std::string req = DTEEncoder::encode(DTECommand::DUMPD_REQ);
	std::string resp;

	mock().expectOneCall("num_entries").onObject(mock_sensor_log).andReturnValue(1);
	mock().expectOneCall("read").onObject(mock_sensor_log).withIntParameter("index", 0).ignoreOtherParameters();

	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;DUMPD#0AC;AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\r", resp.c_str());

	// Check last 16 are retrieved when more than 16 entries are in the log file
	mock().expectOneCall("num_entries").onObject(mock_sensor_log).andReturnValue(20);
	for (unsigned int i = 0; i < 16; i++)
		mock().expectOneCall("read").onObject(mock_sensor_log).withIntParameter("index", i+4).ignoreOtherParameters();
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));

	mock().checkExpectations();
}
