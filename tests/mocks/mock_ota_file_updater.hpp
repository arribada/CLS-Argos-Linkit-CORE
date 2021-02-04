#pragma once

#include <stdint.h>

#include "ota_file_updater.hpp"

class MockOTAFileUpdater : public OTAFileUpdater {
public:
	void start_file_transfer(OTAFileIdentifier file_id, const lfs_size_t length, const uint32_t crc32) override {
		mock().actualCall("start_file_transfer").onObject(this).withParameter("file_id", (unsigned int)file_id).withParameter("length", length).withParameter("crc32", crc32);
	}
	void write_file_data(void * const data, lfs_size_t length) {
		mock().actualCall("write_file_data").onObject(this).withParameter("data", data).withParameter("length", length);
	}
	void abort_file_transfer() {
		mock().actualCall("abort_file_transfer").onObject(this);
	}
	void complete_file_transfer() {
		mock().actualCall("complete_file_transfer").onObject(this);
	}
	void apply_file_update() {
		mock().actualCall("apply_file_update").onObject(this);
	}
};
