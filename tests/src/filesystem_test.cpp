#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "filesystem.hpp"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)


TEST_GROUP(RamFileSystem)
{
	RamFlash *ram_flash;
	LFSFileSystem *fs;
	uint8_t wr_buffer[128];
	uint8_t rd_buffer[128];

	void setup() {
		ram_flash = new RamFlash(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
		fs = new LFSFileSystem(ram_flash);
		fs->format();
		fs->mount();
		for (unsigned int i = 0; i < sizeof(wr_buffer); i++)
			wr_buffer[i] = (uint8_t)i;
		std::memset(rd_buffer, 0, sizeof(rd_buffer));
	}

	void teardown() {
		fs->umount();
		delete fs;
		delete ram_flash;
	}
};


TEST(RamFileSystem, FileClassOperations)
{
	LFSFile *f = new LFSFile(fs, "test", LFS_O_WRONLY | LFS_O_CREAT);

	// Populate file with data
	CHECK_EQUAL(sizeof(wr_buffer), f->write(wr_buffer, sizeof(wr_buffer)));
	CHECK_EQUAL(f->size(), sizeof(wr_buffer));
	delete f;  // Will force a close of the file

	// Read back the data
	f = new LFSFile(fs, "test", LFS_O_RDONLY);
	CHECK_EQUAL(sizeof(rd_buffer), f->read(rd_buffer, sizeof(rd_buffer)));
	MEMCMP_EQUAL(wr_buffer, rd_buffer, sizeof(rd_buffer));

	// Check seek works from 50% of file
	f->seek(sizeof(rd_buffer)/2);
	CHECK_EQUAL(sizeof(rd_buffer)/2, f->read(rd_buffer, sizeof(rd_buffer)/2));
	MEMCMP_EQUAL(&wr_buffer[sizeof(rd_buffer)/2], rd_buffer, sizeof(rd_buffer)/2);
	std::memset(rd_buffer, 0, sizeof(rd_buffer));

	// Check reading past the end of the file terminates early
	f->seek(sizeof(rd_buffer)/2);
	CHECK_EQUAL(sizeof(rd_buffer)/2, f->read(rd_buffer, sizeof(rd_buffer)));

	// Check EOF condition
	CHECK_EQUAL(0, f->read(rd_buffer, sizeof(rd_buffer)));

	delete f;

	// Format the disk
	fs->format();

	// Should throw an exception as unable to open file
	CHECK_THROWS(int, new LFSFile(fs, "test", LFS_O_RDONLY));
}

TEST(RamFileSystem, CircularFileClassOperations)
{
	LFSCircularFile *f = new LFSCircularFile(fs, "test", LFS_O_WRONLY | LFS_O_CREAT, 16);

	// Write 50% of file size (offset=50%)
	CHECK_EQUAL(8, f->write(wr_buffer, 8));
	CHECK_EQUAL(f->get_offset(), 8);

	// Write remainining 50% of file size (offset=0%)
	CHECK_EQUAL(8, f->write(&wr_buffer[8], 8));
	CHECK_EQUAL(f->get_offset(), 0);

	// Write another 50% (offset=50%)
	CHECK_EQUAL(8, f->write(&wr_buffer[16], 8));
	CHECK_EQUAL(f->get_offset(), 8);
	delete f;

	// File closed @offset=50%

	f = new LFSCircularFile(fs, "test", LFS_O_RDONLY, 16);
	// Read from offset=50%
	CHECK_EQUAL(8, f->read(rd_buffer, 8));
	MEMCMP_EQUAL(&wr_buffer[8], rd_buffer, 8);
	// Read from offset=0%
	CHECK_EQUAL(8, f->read(rd_buffer, 8));
	MEMCMP_EQUAL(&wr_buffer[16], rd_buffer, 8);
	// Read from offset=50%
	CHECK_EQUAL(8, f->read(rd_buffer, 8));
	MEMCMP_EQUAL(&wr_buffer[8], rd_buffer, 8);
	delete f;

	// Previous open as RDONLY should not modify the stored pointer
	f = new LFSCircularFile(fs, "test", LFS_O_WRONLY, 16);
	CHECK_EQUAL(8, f->get_offset());
	delete f;
}
