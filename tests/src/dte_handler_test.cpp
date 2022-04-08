#include <sstream>
#include <iostream>
#include <fstream>
#include <string>

#include "gps_scheduler.hpp"
#include "sys_log.hpp"
#include "dte_handler.hpp"
#include "config_store_fs.hpp"
#include "fake_memory_access.hpp"
#include "fake_battery_mon.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "mock_logger.hpp"
#include "previpass.h"
#include "binascii.hpp"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)


extern FileSystem *main_filesystem;
extern ConfigurationStore *configuration_store;
extern MemoryAccess *memory_access;
extern Logger *sensor_log;
extern Logger *system_log;
extern BatteryMonitor *battery_monitor;

TEST_GROUP(DTEHandler)
{
	RamFlash *ram_flash;
	LFSFileSystem *ram_filesystem;
	LFSConfigurationStore *store;
	FakeMemoryAccess *fake_memory_access;
	MockLog *mock_system_log;
	MockLog *mock_sensor_log;
	DTEHandler *dte_handler;
	FakeBatteryMonitor *fake_battery_monitor;
	GPSLogFormatter gps_log_formatter;
	SysLogFormatter sys_log_formatter;

	void setup() {
		ram_flash = new RamFlash(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
		ram_filesystem = new LFSFileSystem(ram_flash);
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
		system_log->set_log_formatter(&sys_log_formatter);
		mock_sensor_log = new MockLog;
		sensor_log = mock_sensor_log;
		sensor_log->set_log_formatter(&gps_log_formatter);
		dte_handler = new DTEHandler();
		fake_battery_monitor = new FakeBatteryMonitor();
		battery_monitor = fake_battery_monitor;
		fake_battery_monitor->m_level = 0U;
		fake_battery_monitor->m_voltage = 0U;
	}

	void teardown() {
		delete dte_handler;
		delete mock_sensor_log;
		delete mock_system_log;
		delete fake_memory_access;
		delete fake_battery_monitor;
		delete store;
		ram_filesystem->umount();
		delete ram_filesystem;
	}

	std::string read_file_into_string(std::string path) {
	    std::ifstream input_file(path);
	    return std::string((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
	}

	std::string read_paspw_file(std::string paspw_file) {
		std::string paspw_data = read_file_into_string(paspw_file);
		std::string buffer;

		// Each packet is of the form "0000..........." so we iteratively
		// find each packet and concatenate it into a working buffer
		size_t pos = 0;
		while (1) {
			// Find start of packet
			pos = paspw_data.find("\"0000", pos);
			if (pos == std::string::npos)
				break;
			// Find end of packet
			pos++;
			size_t end = paspw_data.find("\"", pos);
			if (end == std::string::npos)
				break;
			std::string s = paspw_data.substr(pos, end - pos);
			if (s.size() > 30) // Filter out any unsupported packet types
				buffer += s;
		}

		// Unhexlify and decode the packet
		return buffer;
	}

};


TEST(DTEHandler, PARML_REQ)
{
	std::string resp;
	std::string req = DTEEncoder::encode(DTECommand::PARML_REQ);
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PARML#215;IDP12,IDT06,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP11,ART03,ARP03,ARP04,ARP05,ARP01,ARP19,ARP18,GNP01,ARP11,ARP16,GNP02,GNP03,GNP05,UNP01,UNP02,UNP03,LBP01,LBP02,LBP03,ARP06,LBP04,LBP05,LBP06,ARP12,LBP07,LBP08,LBP09,UNP04,PPP01,PPP02,PPP03,PPP04,PPP05,PPP06,GNP09,GNP10,GNP11,GNP20,GNP21,GNP22,GNP23,ARP30,LDP01,ARP31,ARP32,ARP33,ARP34,ART10,ART11,GNP24,LBP10,LBP11,ZOP01,ZOP04,ZOP05,ZOP06,ZOP08,ZOP09,ZOP10,ZOP11,ZOP12,ZOP13,ZOP14,ZOP15,ZOP16,ZOP17,ZOP18,ZOP19,ZOP20,CTP01,CTP02,CTP03,CTP04,IDT04,POT06,ARP35,IDT10,GNP25,UNP10,UNP11\r", resp.c_str());
}

TEST(DTEHandler, PARMW_REQ)
{
	std::string resp;
	std::string req = "$PARMW#007;ARP04=4\r";
	CHECK_TRUE(DTEAction::CONFIG_UPDATED == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PARMW#000;\r", resp.c_str());
	CHECK_TRUE(BaseArgosPower::POWER_500_MW == configuration_store->read_param<BaseArgosPower>(ParamID::ARGOS_POWER));
}

TEST(DTEHandler, PARMR_REQ)
{
	std::string resp;
	std::string req = "$PARMR#0D7;IDT06,IDP12,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP11,ART03,ARP03,ARP04,ARP05,ARP01,ARP19,ARP18,GNP01,ARP11,ARP16,GNP02,GNP03,GNP05,UNP01,UNP02,UNP03,LBP01,LBP02,LBP03,ARP06,LBP04,LBP05,LBP06,ARP12,LBP07,LBP08,LBP09\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PARMR#173;IDT06=0,IDP12=0,IDT02=SURFACEBOX,IDT03=V0.1,ART01=01/01/1970 00:00:00,ART02=0,POT03=0,POT05=01/01/1970 00:00:00,IDP11=FACTORY,ART03=07/10/2021 22:41:14,ARP03=300,ARP04=4,ARP05=60,ARP01=2,ARP19=0,ARP18=0,GNP01=1,ARP11=1,ARP16=10,GNP02=1,GNP03=2,GNP05=120,UNP01=0,UNP02=1,UNP03=60,LBP01=0,LBP02=10,LBP03=4,ARP06=240,LBP04=2,LBP05=0,LBP06=1,ARP12=4,LBP07=2,LBP08=1,LBP09=120\r", resp.c_str());
}

TEST(DTEHandler, STATR_REQ)
{
	std::string resp;
	std::string req = "$STATR#0D7;IDT06,IDP12,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP11,ART03,ARP03,ARP04,ARP05,ARP01,ARP19,ARP18,GNP01,ARP11,ARP16,GNP02,GNP03,GNP05,UNP01,UNP02,UNP03,LBP01,LBP02,LBP03,ARP06,LBP04,LBP05,LBP06,ARP12,LBP07,LBP08,LBP09\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;STATR#173;IDT06=0,IDP12=0,IDT02=SURFACEBOX,IDT03=V0.1,ART01=01/01/1970 00:00:00,ART02=0,POT03=0,POT05=01/01/1970 00:00:00,IDP11=FACTORY,ART03=07/10/2021 22:41:14,ARP03=300,ARP04=4,ARP05=60,ARP01=2,ARP19=0,ARP18=0,GNP01=1,ARP11=1,ARP16=10,GNP02=1,GNP03=2,GNP05=120,UNP01=0,UNP02=1,UNP03=60,LBP01=0,LBP02=10,LBP03=4,ARP06=240,LBP04=2,LBP05=0,LBP06=1,ARP12=4,LBP07=2,LBP08=1,LBP09=120\r", resp.c_str());
}

TEST(DTEHandler, STATR_REQ_CheckEmptyRequest)
{
	std::string resp;
	std::string req = "$STATR#000;\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;STATR#0B9;IDT06=0,IDT02=SURFACEBOX,IDT03=V0.1,ART01=01/01/1970 00:00:00,ART02=0,POT03=0,POT05=01/01/1970 00:00:00,ART03=07/10/2021 22:41:14,ART10=0,ART11=0,IDT04=SIMULATOR,POT06=0,IDT10=305419896\r", resp.c_str());
}

TEST(DTEHandler, PARMR_REQ_CheckEmptyRequest)
{
	std::string resp;
	std::string req = "$PARMR#000;\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PARMR#2EA;IDP12=0,IDP11=FACTORY,ARP03=300,ARP04=4,ARP05=60,ARP01=2,ARP19=0,ARP18=0,GNP01=1,ARP11=1,ARP16=10,GNP02=1,GNP03=2,GNP05=120,UNP01=0,UNP02=1,UNP03=60,LBP01=0,LBP02=10,LBP03=4,ARP06=240,LBP04=2,LBP05=0,LBP06=1,ARP12=4,LBP07=2,LBP08=1,LBP09=120,UNP04=60,PPP01=15,PPP02=90,PPP03=30,PPP04=1000,PPP05=300,PPP06=10,GNP09=530,GNP10=3,GNP11=0,GNP20=1,GNP21=5,GNP22=1,GNP23=60,ARP30=1,LDP01=1,ARP31=1,ARP32=1,ARP33=900,ARP34=30,GNP24=1,LBP10=5,LBP11=4,ZOP01=1,ZOP04=0,ZOP05=1,ZOP06=01/01/2020 00:00:00,ZOP08=1,ZOP09=3,ZOP10=240,ZOP11=2,ZOP12=16777215,ZOP13=0,ZOP14=3600,ZOP15=2,ZOP16=5,ZOP17=240,ZOP18=-123.392,ZOP19=-48.8752,ZOP20=1000,CTP01=0,CTP02=FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF,CTP03=0,CTP04=60,ARP35=5,GNP25=0,UNP10=0,UNP11=1.1\r", resp.c_str());
}

TEST(DTEHandler, PROFW_PROFR_REQ)
{
	std::string resp;
	std::string req = "$PROFW#018;Profile Name For Tracker\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PROFW#000;\r", resp.c_str());
	req = "$PROFR#000;\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PROFR#018;Profile Name For Tracker\r", resp.c_str());
}

TEST(DTEHandler, SECUR_REQ)
{
	std::string resp;
	std::string req = "$SECUR#004;1234\r";
	CHECK_TRUE(DTEAction::SECUR == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;SECUR#000;\r", resp.c_str());
}

TEST(DTEHandler, ERASE_REQ)
{
	std::string resp;
	std::string req = "$ERASE#001;3\r";
	mock().expectOneCall("truncate").onObject(mock_system_log).andReturnValue(0);
	mock().expectOneCall("truncate").onObject(mock_sensor_log).andReturnValue(0);
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;ERASE#000;\r", resp.c_str());

	req = "$ERASE#001;1\r";
	mock().expectOneCall("truncate").onObject(mock_sensor_log).andReturnValue(0);
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;ERASE#000;\r", resp.c_str());

	req = "$ERASE#001;2\r";
	mock().expectOneCall("truncate").onObject(mock_system_log).andReturnValue(0);
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;ERASE#000;\r", resp.c_str());

	req = "$ERASE#001;0\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$N;ERASE#001;5\r", resp.c_str());
}

TEST(DTEHandler, RSTVW_REQ)
{
	// Increment TX_COUNTER to 1
	configuration_store->increment_tx_counter();
	unsigned int tx_counter = configuration_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1U, tx_counter);

	// This should reset TX_COUNTER to zero
	std::string resp;
	std::string req = "$RSTVW#001;1\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));

	STRCMP_EQUAL("$O;RSTVW#000;\r", resp.c_str());

	tx_counter = configuration_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(0U, tx_counter);

	// Increment RX_COUNTER to 1
	configuration_store->increment_rx_counter();
	unsigned int rx_counter = configuration_store->read_param<unsigned int>(ParamID::ARGOS_RX_COUNTER);
	CHECK_EQUAL(1U, rx_counter);

	// This should reset RX_COUNTER to zero
	req = "$RSTVW#001;3\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;RSTVW#000;\r", resp.c_str());

	rx_counter = configuration_store->read_param<unsigned int>(ParamID::ARGOS_RX_COUNTER);
	CHECK_EQUAL(0U, rx_counter);

	// Increment RX_TIME to 1
	configuration_store->increment_rx_time(1);
	unsigned int rx_time = configuration_store->read_param<unsigned int>(ParamID::ARGOS_RX_TIME);
	CHECK_EQUAL(1U, rx_time);

	// This should reset RX_TIME to zero
	req = "$RSTVW#001;4\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;RSTVW#000;\r", resp.c_str());

	rx_time = configuration_store->read_param<unsigned int>(ParamID::ARGOS_RX_TIME);
	CHECK_EQUAL(0U, rx_time);
}

TEST(DTEHandler, RSTBW_REQ)
{
	std::string resp;
	std::string req = "$RSTBW#000;\r";
	CHECK_TRUE(DTEAction::RESET == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;RSTBW#000;\r", resp.c_str());
}

TEST(DTEHandler, FACTW_REQ)
{
	std::string resp;
	std::string req = "$FACTW#000;\r";
	CHECK_TRUE(DTEAction::FACTR == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;FACTW#000;\r", resp.c_str());
}

TEST(DTEHandler, DUMPM_REQ)
{
	std::string resp;
	std::string req = "$DUMPM#007;100,200\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;DUMPM#2AC;AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/wABAgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICEiIyQlJicoKSorLC0uLzAxMjM0NTY3ODk6Ozw9Pj9AQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVpbXF1eX2BhYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ent8fX5/gIGCg4SFhoeIiYqLjI2Oj5CRkpOUlZaXmJmam5ydnp+goaKjpKWmp6ipqqusra6vsLGys7S1tre4ubq7vL2+v8DBwsPExcbHyMnKy8zNzs/Q0dLT1NXW19jZ2tvc3d7f4OHi4+Tl5ufo6err7O3u7/Dx8vP09fb3+Pn6+/z9/v8=\r", resp.c_str());
}

TEST(DTEHandler, PASPW_REQ_DecodeDayOfYearWiderThan8Bits)
{
	// Supplied by CLS
	std::string allcast_ref = read_paspw_file("data/incorrect_aop.json");
	std::string allcast_binary;

	// Transcode to binary
	allcast_binary = Binascii::unhexlify(allcast_ref);

	BaseRawData paspw_raw = {0, 0, allcast_binary };

	std::string resp;
	std::string req = DTEEncoder::encode(DTECommand::PASPW_REQ, paspw_raw);
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PASPW#000;\r", resp.c_str());

	// Get last AOP date
	req = "$PARMR#005;ART03\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PARMR#019;ART03=18/09/2021 23:09:10\r", resp.c_str());
}

TEST(DTEHandler, DUMPD_REQ_SensorLog)
{
	DTECommand command;
	std::vector<ParamID> params;
	std::vector<ParamValue> param_values;
	std::vector<BaseType> arg_list;
	unsigned int error_code;
	std::string req = DTEEncoder::encode(DTECommand::DUMPD_REQ, BaseLogDType::SENSOR);
	std::string resp;

	// Empty log file should just output a CSV header
	mock().expectOneCall("num_entries").onObject(mock_sensor_log).andReturnValue(0);
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));

	// Decode the response and check the contents
	DTEDecoder::decode(resp, command, error_code, arg_list, params, param_values);
	CHECK_TRUE(DTECommand::DUMPD_RESP == command);
	STRCMP_EQUAL("log_datetime,batt_voltage,iTOW,fix_datetime,valid,onTime,ttff,fixType,flags,flags2,flags3,numSV,lon,lat,height,hMSL,hAcc,vAcc,velN,velE,velD,gSpeed,headMot,sAcc,headAcc,pDOP,vDOP,hDOP,headVeh\r\n",
			std::get<std::string>(arg_list[2]).c_str());

	// Check N entries are retrieved requiring two passes
	mock().expectOneCall("num_entries").onObject(mock_sensor_log).andReturnValue(12);
	for (unsigned int i = 0; i < 8; i++)
		mock().expectOneCall("read").onObject(mock_sensor_log).withIntParameter("index", i).ignoreOtherParameters();
	CHECK_TRUE(DTEAction::AGAIN == dte_handler->handle_dte_message(req, resp));

	arg_list.clear();
	DTEDecoder::decode(resp, command, error_code, arg_list, params, param_values);
	CHECK_TRUE(DTECommand::DUMPD_RESP == command);
	printf(std::get<std::string>(arg_list[2]).c_str());

	mock().expectOneCall("num_entries").onObject(mock_sensor_log).andReturnValue(12);
	for (unsigned int i = 8; i < 12; i++)
		mock().expectOneCall("read").onObject(mock_sensor_log).withIntParameter("index", i).ignoreOtherParameters();
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));

	arg_list.clear();
	DTEDecoder::decode(resp, command, error_code, arg_list, params, param_values);
	CHECK_TRUE(DTECommand::DUMPD_RESP == command);
	printf(std::get<std::string>(arg_list[2]).c_str());

	mock().checkExpectations();
}

TEST(DTEHandler, DUMPD_REQ_InternalLog)
{
	DTECommand command;
	std::vector<ParamID> params;
	std::vector<ParamValue> param_values;
	std::vector<BaseType> arg_list;
	unsigned int error_code;
	std::string req = DTEEncoder::encode(DTECommand::DUMPD_REQ, BaseLogDType::INTERNAL);
	std::string resp;

	// Empty log file should just output a CSV header
	mock().expectOneCall("num_entries").onObject(mock_system_log).andReturnValue(0);
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));

	// Decode the response and check the contents
	DTEDecoder::decode(resp, command, error_code, arg_list, params, param_values);
	CHECK_TRUE(DTECommand::DUMPD_RESP == command);
	STRCMP_EQUAL("log_datetime,log_level,message\r\n",
			std::get<std::string>(arg_list[2]).c_str());

	// Check N entries are retrieved requiring two passes
	mock().expectOneCall("num_entries").onObject(mock_system_log).andReturnValue(12);
	for (unsigned int i = 0; i < 8; i++)
		mock().expectOneCall("read").onObject(mock_system_log).withIntParameter("index", i).ignoreOtherParameters();
	CHECK_TRUE(DTEAction::AGAIN == dte_handler->handle_dte_message(req, resp));

	arg_list.clear();
	DTEDecoder::decode(resp, command, error_code, arg_list, params, param_values);
	CHECK_TRUE(DTECommand::DUMPD_RESP == command);
	printf(std::get<std::string>(arg_list[2]).c_str());

	mock().expectOneCall("num_entries").onObject(mock_system_log).andReturnValue(12);
	for (unsigned int i = 8; i < 12; i++)
		mock().expectOneCall("read").onObject(mock_system_log).withIntParameter("index", i).ignoreOtherParameters();
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));

	arg_list.clear();
	DTEDecoder::decode(resp, command, error_code, arg_list, params, param_values);
	CHECK_TRUE(DTECommand::DUMPD_RESP == command);
	printf(std::get<std::string>(arg_list[2]).c_str());

	mock().checkExpectations();
}

TEST(DTEHandler, WritingReadOnlyAttributesIsIgnored)
{
	std::string resp;
	std::string req = "$PARMW#007;ART02=1\r";
	CHECK_TRUE(DTEAction::CONFIG_UPDATED == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PARMW#000;\r", resp.c_str());

	unsigned int tx_counter = configuration_store->read_param<unsigned int>(ParamID::TX_COUNTER);

	CHECK_EQUAL(0, tx_counter);
}

TEST(DTEHandler, WritingOutOfRangeValue)
{
	std::string resp;
	std::string req = "$PARMW#009;PPP01=-12\r";
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$N;PARMW#001;5\r", resp.c_str());
}


TEST(DTEHandler, GenerateDefaultPassPredictFile)
{
	std::string allcast_ref = read_paspw_file("data/default_aop.json");
	std::string allcast_binary;

	// Transcode to binary
	allcast_binary = Binascii::unhexlify(allcast_ref);

	BaseRawData paspw_raw = {0, 0, allcast_binary };

	std::string resp;
	std::string req = DTEEncoder::encode(DTECommand::PASPW_REQ, paspw_raw);
	CHECK_TRUE(DTEAction::NONE == dte_handler->handle_dte_message(req, resp));
	STRCMP_EQUAL("$O;PASPW#000;\r", resp.c_str());

	// Read out the prepass file
	BasePassPredict pp;
	pp = configuration_store->read_pass_predict();
	for (unsigned int i = 0; i < pp.num_records; i++) {
		printf("{ 0x%1x, %u, (SatDownlinkStatus_t)%u, (SatUplinkStatus_t)%u, { %u, %u, %u, %u, %u, %u }, %f, %f, %f, %f, %f, %f },\n",
				pp.records[i].satHexId,
				pp.records[i].satDcsId,
				pp.records[i].downlinkStatus,
				pp.records[i].uplinkStatus,
				pp.records[i].bulletin.year,
				pp.records[i].bulletin.month,
				pp.records[i].bulletin.day,
				pp.records[i].bulletin.hour,
				pp.records[i].bulletin.minute,
				pp.records[i].bulletin.second,
				(double)pp.records[i].semiMajorAxisKm,
				(double)pp.records[i].inclinationDeg,
				(double)pp.records[i].ascNodeLongitudeDeg,
				(double)pp.records[i].ascNodeDriftDeg,
				(double)pp.records[i].orbitPeriodMin,
				(double)pp.records[i].semiMajorAxisDriftMeterPerDay
				);
	}
}
