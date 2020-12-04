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
	CHECK_TRUE(DTEAction::NONE == DTEHandler::handle_dte_message(configuration_store, req, resp));
	CHECK_EQUAL("$O;PARML#0D7;IDT06,IDT07,IDT02,IDT03,ART01,ART02,POT03,POT05,IDP09,ART03,ARP03,ARP04,ARP05,ARP01,ARP19,ARP18,GNP01,ARP11,ARP16,GNP02,GNP03,GNP05,UNP01,UNP02,UNP03,LBP01,LBP02,LBP03,ARP06,LBP04,LBP05,LBP06,ARP12,LBP07,LBP08,LBP09\r", resp);
	std::cout << resp << "\n";
}
