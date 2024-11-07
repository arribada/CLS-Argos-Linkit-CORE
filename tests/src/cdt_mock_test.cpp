#include <stdexcept>

#include "filesystem.hpp"
#include "cdt.hpp"
#include "mock_ad5933.hpp"
#include "mock_ms58xx.hpp"
#include "calibration.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)
#define MAX_FILE_SIZE (4*1024*1024)


extern FileSystem *main_filesystem;
static LFSFileSystem *ram_filesystem;


TEST_GROUP(CDTMock)
{
	RamFlash *ram_flash;
	MockAD5933 m_ad5933;
	MockMS58xx m_ms58xx;

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

	double calculate(unsigned int samples, int16_t real[2], int16_t imag[2], double GF, double CA, double CB, double CC) {
		double impedence = 0;
		for (unsigned int i = 0; i < samples; i++) {
			double magnitude = std::sqrt(std::pow((double)real[i], 2) + std::pow((double)imag[i], 2));
			impedence += 1 / (GF * magnitude);
		}
		impedence /= samples;
		double conduino = 1 / impedence * 1000;
		return 1000 * (CA * conduino * conduino + CB * conduino + CC);
	}
};


TEST(CDTMock, CDTConductivityCalculation)
{
	CDT cdt(m_ms58xx, m_ad5933);
	int16_t real[2] = {2000, 2020};
	int16_t imag[2] = {-1900, -1950};
	mock().expectOneCall("start").onObject(&m_ad5933).
		withUnsignedIntParameter("frequency", 90000).
		withUnsignedIntParameter("vrange", 3);
	mock().expectOneCall("stop").onObject(&m_ad5933);
	mock().expectOneCall("get_real_imaginary.real").onObject(&m_ad5933).
			andReturnValue(real[0]);
	mock().expectOneCall("get_real_imaginary.imag").onObject(&m_ad5933).
			andReturnValue(imag[0]);
	mock().expectOneCall("get_real_imaginary.real").onObject(&m_ad5933).
			andReturnValue(real[1]);
	mock().expectOneCall("get_real_imaginary.imag").onObject(&m_ad5933).
			andReturnValue(imag[1]);
	double actual = cdt.read(0);
	double ref = calculate(2, real, imag, 10.95E-6, 0.0011, 0.9095, 0.4696);
	CHECK_EQUAL(ref, actual);
}
