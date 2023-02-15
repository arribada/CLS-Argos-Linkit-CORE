#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "ubx_ano.hpp"
#include "fake_rtc.hpp"

extern "C" {
#include <stdio.h>
#include <stdlib.h>
}

extern RTC *rtc;

class PlainFile : public File {
private:
	FILE *fp;

public:
	PlainFile(const char *path, const char *mode) {
		fp = fopen(path, mode);
		//DEBUG_TRACE("PlainFile(%s, %s)=%p", path, mode, fp);
		if (fp == NULL) throw;
	}
	virtual ~PlainFile() { fclose(fp); }
	lfs_ssize_t read(void *buffer, lfs_size_t size) override {
		return fread(buffer, 1, size, fp);
	}
	lfs_ssize_t write(void *buffer, lfs_size_t size) override {
		return fwrite(buffer, 1, size, fp);
	}
	lfs_soff_t seek(lfs_soff_t off, int whence=LFS_SEEK_SET) override {
		(void)whence;
		return fseek(fp, off, SEEK_SET);
	}
	lfs_soff_t tell() override {
		return ftell(fp);
	}
	int flush() override {
		return 0;
	}
	lfs_soff_t size() {
		return 0;
	}
};


TEST_GROUP(UBXANO)
{
	FakeRTC *fake_rtc;

	void setup() {
		fake_rtc = new FakeRTC;
		rtc = fake_rtc;
	}

	void teardown() {
		delete fake_rtc;
	}
};

TEST(UBXANO, ANO4thMay2022)
{
	fake_rtc->settime(1651665413);

	PlainFile m_ano_file("./data/mgaoffline.ubx", "rb");
	UBXAssistNowOffline iterator(m_ano_file);
	unsigned int i = 0;

	while (iterator.value()) {
		uint8_t *ptr = (uint8_t *)iterator.value();
		UBX::MGA::MSG_ANO *ano = (UBX::MGA::MSG_ANO *)&ptr[sizeof(UBX::Header)];
		//DEBUG_TRACE("Entry %u dd/mm/yy=%02u/%02u/%02u svId=%u", i, (unsigned int)ano->day, (unsigned int)ano->month, (unsigned int)ano->year, (unsigned int)ano->svId);
		CHECK_EQUAL(4, ano->day);
		iterator.next();
		i++;
	}
	CHECK_EQUAL(30, i);
}

TEST(UBXANO, ANO17thMay2022)
{
	fake_rtc->settime(1661665413);

	PlainFile m_ano_file("./data/mgaoffline.ubx", "rb");
	UBXAssistNowOffline iterator(m_ano_file);
	unsigned int i = 0;

	while (iterator.value()) {
		uint8_t *ptr = (uint8_t *)iterator.value();
		UBX::MGA::MSG_ANO *ano = (UBX::MGA::MSG_ANO *)&ptr[sizeof(UBX::Header)];
		//DEBUG_TRACE("Entry %u dd/mm/yy=%02u/%02u/%02u svId=%u", i, (unsigned int)ano->day, (unsigned int)ano->month, (unsigned int)ano->year, (unsigned int)ano->svId);
		CHECK_EQUAL(17, ano->day);
		iterator.next();
		i++;
	}

	CHECK_EQUAL(30, i);
}

TEST(UBXANO, EmptyFile)
{
	fake_rtc->settime(1661665413);

	PlainFile m_ano_file("./data/mgaoffline_empty.ubx", "rb");
	UBXAssistNowOffline iterator(m_ano_file);
	unsigned int i = 0;

	while (iterator.value()) {
		uint8_t *ptr = (uint8_t *)iterator.value();
		UBX::MGA::MSG_ANO *ano = (UBX::MGA::MSG_ANO *)&ptr[sizeof(UBX::Header)];
		//DEBUG_TRACE("Entry %u dd/mm/yy=%02u/%02u/%02u svId=%u", i, (unsigned int)ano->day, (unsigned int)ano->month, (unsigned int)ano->year, (unsigned int)ano->svId);
		CHECK_EQUAL(17, ano->day);
		iterator.next();
		i++;
	}

	CHECK_EQUAL(0, i);
}

TEST(UBXANO, NoSync)
{
	fake_rtc->settime(1661665413);

	PlainFile m_ano_file("./data/mgaoffline_nosync.ubx", "rb");
	UBXAssistNowOffline iterator(m_ano_file);
	unsigned int i = 0;

	while (iterator.value()) {
		uint8_t *ptr = (uint8_t *)iterator.value();
		UBX::MGA::MSG_ANO *ano = (UBX::MGA::MSG_ANO *)&ptr[sizeof(UBX::Header)];
		//DEBUG_TRACE("Entry %u dd/mm/yy=%02u/%02u/%02u svId=%u", i, (unsigned int)ano->day, (unsigned int)ano->month, (unsigned int)ano->year, (unsigned int)ano->svId);
		CHECK_EQUAL(17, ano->day);
		iterator.next();
		i++;
	}

	CHECK_EQUAL(0, i);
}
