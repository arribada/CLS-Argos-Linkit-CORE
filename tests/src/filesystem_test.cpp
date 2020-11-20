#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "filesystem.hpp"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)


static RamFileSystem *fs;
static uint8_t wr_buffer[128];
static uint8_t rd_buffer[128];

TEST_GROUP(RamFileSystem)
{
	void setup() {
		fs = new RamFileSystem(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
		fs->format();
		fs->mount();
		for (unsigned int i = 0; i < sizeof(wr_buffer); i++)
			wr_buffer[i] = (uint8_t)i;
		std::memset(rd_buffer, 0, sizeof(rd_buffer));
	}

	void teardown() {
		delete fs;
	}
};


TEST(RamFileSystem, PlainFileOperations)
{
	File *f1 = new File(fs, "test", LFS_O_RDWR | LFS_O_CREAT);
	f1->write(wr_buffer, sizeof(wr_buffer));
	CHECK_EQUAL(f1->size(), sizeof(wr_buffer));
	delete f1;
}

TEST(RamFileSystem, CircularFileWrappingAndWriteOffsetPersistence)
{
	CircularFile *f1 = new CircularFile(fs, "test", LFS_O_RDWR | LFS_O_CREAT, 16);
	f1->write(wr_buffer, 8);
	CHECK_EQUAL(f1->m_offset, 8);
	f1->write(wr_buffer, 8);
	CHECK_EQUAL(f1->m_offset, 0);
	f1->write(wr_buffer, 8);
	CHECK_EQUAL(f1->m_offset, 8);
	delete f1;
	CircularFile *f2 = new CircularFile(fs, "test", LFS_O_RDWR, 16);
	CHECK_EQUAL(f2->m_offset, 8);
	delete f2;
}
