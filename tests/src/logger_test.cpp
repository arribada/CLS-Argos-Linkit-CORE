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
	FsLog *fs_sensor;
	FsLog *fs_system;

	void setup() {
		ram_flash = new RamFlash(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
		ram_filesystem = new LFSFileSystem(ram_flash);
		ram_filesystem->format();
		ram_filesystem->mount();
		fs_sensor = new FsLog(ram_filesystem, "sensor.log", (1024*1024));
		fs_system = new FsLog(ram_filesystem, "system.log", (1024*1024));
	}

	void teardown() {
		delete fs_sensor;
		delete fs_system;
		ram_filesystem->umount();
		delete ram_filesystem;
		delete ram_flash;
	}
};


TEST(Logger, CanCreateAndWriteToLogFiles)
{
	LoggerManager::create();
	CHECK_EQUAL(0, fs_sensor->num_entries());
	CHECK_EQUAL(0, fs_system->num_entries());

	LogEntry dummy;
	fs_sensor->write(&dummy);
	fs_sensor->write(&dummy);
	fs_sensor->write(&dummy);
	fs_sensor->write(&dummy);
	fs_system->write(&dummy);

	CHECK_EQUAL(4, fs_sensor->num_entries());
	CHECK_EQUAL(1, fs_system->num_entries());

	// Should not re-create files
	LoggerManager::create();

	CHECK_EQUAL(4, fs_sensor->num_entries());
	CHECK_EQUAL(1, fs_system->num_entries());
}


TEST(Logger, CanFillAndWrapLogFile)
{
	LoggerManager::create();

	CHECK_EQUAL(0, fs_sensor->num_entries());
	CHECK_EQUAL(0, fs_system->num_entries());

	LogEntry dummy;

	for (unsigned int i = 0; i < (1024*1024)/sizeof(dummy); i++)
		fs_sensor->write(&dummy);

	// This overwrites log chunk index=0, so this chunk will now only have 1 entry it
	// erasing the previously stored entries
	fs_sensor->write(&dummy);

	CHECK_EQUAL(((1024*1024) - 4096 + 128) / sizeof(dummy), fs_sensor->num_entries());
}


TEST(Logger, LogFileIsPersistent)
{
	LoggerManager::create();

	CHECK_EQUAL(0, fs_sensor->num_entries());

	LogEntry dummy;
	fs_sensor->write(&dummy);

	CHECK_EQUAL(1, fs_sensor->num_entries());

	delete fs_sensor;
	fs_sensor = new FsLog(ram_filesystem, "sensor.log", (1024*1024));

	LoggerManager::create();

	CHECK_EQUAL(1, fs_sensor->num_entries());

	// Wrap the log file and make sure wrapped file property is also persistent
	for (unsigned int i = 0; i < (1024*1024)/sizeof(dummy); i++)
		fs_sensor->write(&dummy);
	unsigned int num_entries = fs_sensor->num_entries();

	delete fs_sensor;
	fs_sensor = new FsLog(ram_filesystem, "sensor.log", (1024*1024));

	LoggerManager::create();

	CHECK_EQUAL(num_entries, fs_sensor->num_entries());
}

TEST(Logger, ReadBackWrittenLogEntriesAfterReset)
{
	LoggerManager::create();

	LogEntry dummy;

	// Fill the log file
	for (unsigned int i = 0; i < (1024*1024)/sizeof(dummy); i++) {
		*((unsigned int *)dummy.data) = i;
		fs_sensor->write(&dummy);
	}

	// Reset
	delete fs_sensor;
	fs_sensor = new FsLog(ram_filesystem, "sensor.log", (1024*1024));

	LoggerManager::create();

	// Read back and check counter values match
	for (unsigned int i = 0; i < (1024*1024)/sizeof(dummy); i++) {
		fs_sensor->read(&dummy, i);
		CHECK_EQUAL(i, *((unsigned int *)dummy.data));
	}

	// Force wrap on the log file which overwrites log chunk index=0
	*((unsigned int *)dummy.data) = (1024*1024)/sizeof(dummy) + 1;
	fs_sensor->write(&dummy);

	// First entry should now be at log chunk index=1
	for (unsigned int i = 0; i < ((1024*1024) - 4096)/sizeof(dummy); i++) {
		fs_sensor->read(&dummy, i);
		CHECK_EQUAL(i + 32, *((unsigned int *)dummy.data));
	}

	// Now read back last remaining log entry at log chunk index=0
	fs_sensor->read(&dummy, fs_sensor->num_entries() - 1);
	CHECK_EQUAL((1024*1024)/sizeof(dummy) + 1, *((unsigned int *)dummy.data));
}

TEST(Logger, ConcurrentWritingAndReading)
{
	LoggerManager::create();

	LogEntry dummy;

	// Interleave write and read operations
	for (unsigned int i = 0; i < (1024*1024)/sizeof(dummy); i++) {
		*((unsigned int *)dummy.data) = i;
		fs_sensor->write(&dummy);
		*((unsigned int *)dummy.data) = 0;
		fs_sensor->read(&dummy, i);
		CHECK_EQUAL(i, *((unsigned int *)dummy.data));
	}
}

TEST(Logger, CorruptedFileAttributeError)
{
	LoggerManager::create();
	delete fs_sensor;

	// Corrupt the file attribute
	unsigned int attr = 0xFFFFFFFF;
	ram_filesystem->set_attr("sensor.log", attr);

	fs_sensor = new FsLog(ram_filesystem, "sensor.log", (1024*1024));

	// Should throw BAD_FILESYSTEM exception
	CHECK_THROWS(ErrorCode, fs_sensor->create());
}

TEST(Logger, MissingLogChunkError)
{
	LoggerManager::create();
	delete fs_sensor;

	// Remove first log chunk
	ram_filesystem->remove("sensor.log.0");

	fs_sensor = new FsLog(ram_filesystem, "sensor.log", (1024*1024));

	// Should throw BAD_FILESYSTEM exception
	CHECK_THROWS(ErrorCode, fs_sensor->create());
}
