#include "fs_log.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)


TEST_GROUP(Logger)
{
	RamFlash *ram_flash;
	LFSFileSystem *ram_filesystem;

	void setup() {
		ram_flash = new RamFlash(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
		ram_filesystem = new LFSFileSystem(ram_flash);
		ram_filesystem->format();
		ram_filesystem->mount();
	}

	void teardown() {
		ram_filesystem->umount();
		delete ram_filesystem;
		delete ram_flash;
	}
};


TEST(Logger, CanCreateAndWriteToLogFiles)
{
	FsLog sensor_log(ram_filesystem, "sensor.log", (1024*1024));
	FsLog system_log(ram_filesystem, "system.log", (1024*1024));

	sensor_log.create();
	system_log.create();

	CHECK_EQUAL(0, sensor_log.num_entries());
	CHECK_EQUAL(0, system_log.num_entries());

	LogEntry dummy;
	sensor_log.write(&dummy);
	sensor_log.write(&dummy);
	sensor_log.write(&dummy);
	sensor_log.write(&dummy);
	system_log.write(&dummy);

	CHECK_EQUAL(4, sensor_log.num_entries());
	CHECK_EQUAL(1, system_log.num_entries());

	// Should not re-create files
	sensor_log.create();
	system_log.create();

	CHECK_EQUAL(4, sensor_log.num_entries());
	CHECK_EQUAL(1, system_log.num_entries());
}
