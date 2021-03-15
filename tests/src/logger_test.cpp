#include "fs_log.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)

extern Logger *sensor_log;
extern Logger *system_log;


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
		sensor_log = fs_sensor;
		fs_system = new FsLog(ram_filesystem, "system.log", (1024*1024));
		system_log = fs_system;
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
	sensor_log->create();
	system_log->create();

	CHECK_EQUAL(0, sensor_log->num_entries());
	CHECK_EQUAL(0, system_log->num_entries());

	LogEntry dummy;
	sensor_log->write(&dummy);
	sensor_log->write(&dummy);
	sensor_log->write(&dummy);
	sensor_log->write(&dummy);
	system_log->write(&dummy);

	CHECK_EQUAL(4, sensor_log->num_entries());
	CHECK_EQUAL(1, system_log->num_entries());

	// Should not re-create files
	sensor_log->create();
	system_log->create();

	CHECK_EQUAL(4, sensor_log->num_entries());
	CHECK_EQUAL(1, system_log->num_entries());
}


TEST(Logger, CanFillAndWrapLogFile)
{
	sensor_log->create();
	system_log->create();

	CHECK_EQUAL(0, sensor_log->num_entries());
	CHECK_EQUAL(0, system_log->num_entries());

	LogEntry dummy;

	for (unsigned int i = 0; i < (1024*1024)/sizeof(dummy); i++)
		sensor_log->write(&dummy);

	// This overwrites log chunk index=0, so this chunk will now only have 1 entry it
	// erasing the previously stored entries
	sensor_log->write(&dummy);

	CHECK_EQUAL(((1024*1024) - 4096 + 128) / sizeof(dummy), sensor_log->num_entries());
}


TEST(Logger, LogFileIsPersistent)
{
	sensor_log->create();

	CHECK_EQUAL(0, sensor_log->num_entries());

	LogEntry dummy;
	sensor_log->write(&dummy);

	CHECK_EQUAL(1, sensor_log->num_entries());

	delete fs_sensor;
	fs_sensor = new FsLog(ram_filesystem, "sensor.log", (1024*1024));
	sensor_log = fs_sensor;

	sensor_log->create();

	CHECK_EQUAL(1, sensor_log->num_entries());

	// Wrap the log file and make sure wrapped file property is also persistent
	for (unsigned int i = 0; i < (1024*1024)/sizeof(dummy); i++)
		sensor_log->write(&dummy);
	unsigned int num_entries = sensor_log->num_entries();

	delete fs_sensor;
	fs_sensor = new FsLog(ram_filesystem, "sensor.log", (1024*1024));
	sensor_log = fs_sensor;

	sensor_log->create();

	CHECK_EQUAL(num_entries, sensor_log->num_entries());
}

TEST(Logger, ReadBackWrittenLogEntriesAfterReset)
{
	sensor_log->create();

	LogEntry dummy;

	// Fill the log file
	for (unsigned int i = 0; i < (1024*1024)/sizeof(dummy); i++) {
		*((unsigned int *)dummy.data) = i;
		sensor_log->write(&dummy);
	}

	// Reset
	delete fs_sensor;
	fs_sensor = new FsLog(ram_filesystem, "sensor.log", (1024*1024));
	sensor_log = fs_sensor;
	sensor_log->create();

	// Read back and check counter values match
	for (unsigned int i = 0; i < (1024*1024)/sizeof(dummy); i++) {
		sensor_log->read(&dummy, i);
		CHECK_EQUAL(i, *((unsigned int *)dummy.data));
	}

	// Force wrap on the log file which overwrites log chunk index=0
	*((unsigned int *)dummy.data) = (1024*1024)/sizeof(dummy) + 1;
	sensor_log->write(&dummy);

	// First entry should now be at log chunk index=1
	for (unsigned int i = 0; i < ((1024*1024) - 4096)/sizeof(dummy); i++) {
		sensor_log->read(&dummy, i);
		CHECK_EQUAL(i + 32, *((unsigned int *)dummy.data));
	}

	// Now read back last remaining log entry at log chunk index=0
	sensor_log->read(&dummy, sensor_log->num_entries() - 1);
	CHECK_EQUAL((1024*1024)/sizeof(dummy) + 1, *((unsigned int *)dummy.data));
}

TEST(Logger, ConcurrentWritingAndReading)
{
	sensor_log->create();

	LogEntry dummy;

	// Interleave write and read operations
	for (unsigned int i = 0; i < (1024*1024)/sizeof(dummy); i++) {
		*((unsigned int *)dummy.data) = i;
		sensor_log->write(&dummy);
		*((unsigned int *)dummy.data) = 0;
		sensor_log->read(&dummy, i);
		CHECK_EQUAL(i, *((unsigned int *)dummy.data));
	}
}

TEST(Logger, CorruptedFileAttributeError)
{
	sensor_log->create();
	delete fs_sensor;

	// Corrupt the file attribute
	unsigned int attr = 0xFFFFFFFF;
	ram_filesystem->set_attr("sensor.log", attr);

	fs_sensor = new FsLog(ram_filesystem, "sensor.log", (1024*1024));
	sensor_log = fs_sensor;

	// Should throw BAD_FILESYSTEM exception
	CHECK_THROWS(ErrorCode, sensor_log->create());
}

TEST(Logger, MissingLogChunkError)
{
	sensor_log->create();
	delete fs_sensor;

	// Remove first log chunk
	ram_filesystem->remove("sensor.log.0");

	fs_sensor = new FsLog(ram_filesystem, "sensor.log", (1024*1024));
	sensor_log = fs_sensor;

	// Should throw BAD_FILESYSTEM exception
	CHECK_THROWS(ErrorCode, sensor_log->create());
}
