#ifndef __MOCK_LOGGER_HPP_
#define __MOCK_LOGGER_HPP_

#include <cstring>
#include "logger.hpp"
#include "messages.hpp"

class MockLog : public Logger {
public:

	void create() {
		mock().actualCall("create").onObject(this);
	}

	void truncate() {
		mock().actualCall("truncate").onObject(this);
	}

	bool is_ready() {
		return mock().actualCall("is_ready").onObject(this).returnBoolValue();
	}

	void write(void *entry) {
		mock().actualCall("create").onObject(this).withParameter("entry", entry);
	}

	void read(void *entry, int index=0) {
		mock().actualCall("read").onObject(this).withParameter("entry", entry).withParameter("index", index);
		std::memset(entry, 0, sizeof(LogEntry));
	}

	unsigned int num_entries() {
		return mock().actualCall("num_entries").onObject(this).returnIntValue();
	}
};

#endif // __MOCK_LOGGER_HPP_
