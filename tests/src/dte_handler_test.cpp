#include "dte_handler.hpp"
#include "config_store_fs.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)


extern FileSystem *main_filesystem;
extern ConfigurationStore *configuration_store;

TEST_GROUP(DTEHandler)
{
	LFSRamFileSystem *ram_filesystem;
	LFSConfigurationStore *store;

	void setup() {
		ram_filesystem = new LFSRamFileSystem(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
		ram_filesystem->format();
		ram_filesystem->mount();
		main_filesystem = ram_filesystem;
		store = new LFSConfigurationStore();
		store->init();
		configuration_store = store;
	}

	void teardown() {
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
	CHECK_EQUAL("$O;PARML#0D7;IDT06,IDT07,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP09,ART03,ARP03,ARP04,ARP05,ARP01,ARP19,ARP18,GNP01,ARP11,ARP16,GNP02,GNP03,GNP05,UNP01,UNP02,UNP03,LBP01,LBP02,LBP03,ARP06,LBP04,LBP05,LBP06,ARP12,LBP07,LBP08,LBP09\r", resp);
}

TEST(DTEHandler, PARMW_REQ)
{
	std::string resp;
	std::string req = "$PARMW#009;ARP04=500\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	CHECK_EQUAL("$O;PARMW#000;\r", resp);
	CHECK_TRUE(BaseArgosPower::POWER_500_MW == configuration_store->read_param<BaseArgosPower>(ParamID::ARGOS_POWER));
}

TEST(DTEHandler, PARMR_REQ)
{
	std::string resp;
	std::string req = "$PARMR#0D7;IDT06,IDT07,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP09,ART03,ARP03,ARP04,ARP05,ARP01,ARP19,ARP18,GNP01,ARP11,ARP16,GNP02,GNP03,GNP05,UNP01,UNP02,UNP03,LBP01,LBP02,LBP03,ARP06,LBP04,LBP05,LBP06,ARP12,LBP07,LBP08,LBP09\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	CHECK_EQUAL("$O;PARMR#175;IDT06=0,IDT07=0,IDT02=0,IDT03=V0.1,ART01=Thu Jan  1 00:00:00 1970,ART02=0,POT03=0,POT05=Thu Jan  1 00:00:00 1970,IDP09=,ART03=Thu Jan  1 00:00:00 1970,ARP03=399.91,ARP04=750,ARP05=45,ARP01=0,ARP19=1,ARP18=0,GNP01=0,ARP11=10,ARP16=1,GNP02=0,GNP03=2,GNP05=60,UNP01=0,UNP02=1,UNP03=1,LBP01=0,LBP02=0,LBP03=750,ARP06=45,LBP04=0,LBP05=0,LBP06=0,ARP12=60,LBP07=2,LBP08=1,LBP09=60\r", resp);
}

TEST(DTEHandler, PROFW_PROFR_REQ)
{
	std::string resp;
	std::string req = "$PROFW#018;Profile Name For Tracker\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	CHECK_EQUAL("$O;PROFW#000;\r", resp);
	req = "$PROFR#000;\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	CHECK_EQUAL("$O;PROFR#018;Profile Name For Tracker\r", resp);
}
