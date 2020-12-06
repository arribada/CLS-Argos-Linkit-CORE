#include "dte_handler.hpp"
#include "config_store_fs.hpp"
#include "fake_memory_access.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)


extern FileSystem *main_filesystem;
extern ConfigurationStore *configuration_store;
extern MemoryAccess *memory_access;

TEST_GROUP(DTEHandler)
{
	LFSRamFileSystem *ram_filesystem;
	LFSConfigurationStore *store;
	FakeMemoryAccess *fake_memory_access;

	void setup() {
		ram_filesystem = new LFSRamFileSystem(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
		ram_filesystem->format();
		ram_filesystem->mount();
		main_filesystem = ram_filesystem;
		store = new LFSConfigurationStore();
		store->init();
		configuration_store = store;
		fake_memory_access = new FakeMemoryAccess();
		memory_access = fake_memory_access;
	}

	void teardown() {
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

TEST(DTEHandler, SECUR_REQ)
{
	std::string resp;
	std::string req = "$SECUR#000;\r";
	CHECK_TRUE(DTEAction::SECUR == DTEHandler::handle_dte_message(req, resp));
	CHECK_EQUAL("$O;SECUR#000;\r", resp);
}

TEST(DTEHandler, RESET_REQ)
{
	std::string resp;
	std::string req = "$RESET#000;\r";
	CHECK_TRUE(DTEAction::RESET == DTEHandler::handle_dte_message(req, resp));
	CHECK_EQUAL("$O;RESET#000;\r", resp);
}

TEST(DTEHandler, FACTR_REQ)
{
	std::string resp;
	std::string req = "$FACTR#000;\r";
	CHECK_TRUE(DTEAction::FACTR == DTEHandler::handle_dte_message(req, resp));
	CHECK_EQUAL("$O;FACTR#000;\r", resp);
}

TEST(DTEHandler, DUMPM_REQ)
{
	std::string resp;
	std::string req = "$DUMPM#007;100,200\r";
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(req, resp));
	CHECK_EQUAL("$O;DUMPM#2AC;AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/wABAgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICEiIyQlJicoKSorLC0uLzAxMjM0NTY3ODk6Ozw9Pj9AQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVpbXF1eX2BhYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ent8fX5/gIGCg4SFhoeIiYqLjI2Oj5CRkpOUlZaXmJmam5ydnp+goaKjpKWmp6ipqqusra6vsLGys7S1tre4ubq7vL2+v8DBwsPExcbHyMnKy8zNzs/Q0dLT1NXW19jZ2tvc3d7f4OHi4+Tl5ufo6err7O3u7/Dx8vP09fb3+Pn6+/z9/v8=\r", resp);
}
