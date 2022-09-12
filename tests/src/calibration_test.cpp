#include <stdexcept>

#include "filesystem.hpp"
#include "calibration.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)


extern FileSystem *main_filesystem;
static LFSFileSystem *ram_filesystem;


TEST_GROUP(Calibration)
{
	RamFlash *ram_flash;

	void setup() {
		ram_flash = new RamFlash(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
		ram_filesystem = new LFSFileSystem(ram_flash);
		ram_filesystem->format();
		ram_filesystem->mount();
		main_filesystem = ram_filesystem;
	}

	void teardown() {
		ram_filesystem->umount();
		delete ram_filesystem;
		delete ram_flash;
	}
};


TEST(Calibration, CreateCalibrationPopulateSaveAndVerify)
{
	{
		Calibration cal("MYCAL");

		// Initially empty, should raise exception
		CHECK_THROWS(std::out_of_range, cal.read(0));

		for (unsigned int i = 0; i < 10; i++)
			cal.write(i, i);

		// Destructor saves the calibration
	}

	{
		Calibration cal("MYCAL");

		for (unsigned int i = 0; i < 10; i++)
			CHECK_EQUAL(i, cal.read(i));
	}
}


TEST(Calibration, CreateCalibrationWithoutSaving)
{
	{
		Calibration cal("MYCAL");

		for (unsigned int i = 0; i < 10; i++)
			cal.write(i, i);

		Calibration cal2("MYCAL");

		// Was not saved, should raise exception
		CHECK_THROWS(std::out_of_range, cal2.read(0));
	}
}


TEST(Calibration, CreateCalibrationSaveResetAndVerifyEmpty)
{
	{
		Calibration cal("MYCAL");

		for (unsigned int i = 0; i < 10; i++)
			cal.write(i, i);
	}

	{
		Calibration cal("MYCAL");
		cal.reset();
	}

	{
		Calibration cal("MYCAL");

		// Initially empty, should raise exception
		CHECK_THROWS(std::out_of_range, cal.read(0));
	}
}
