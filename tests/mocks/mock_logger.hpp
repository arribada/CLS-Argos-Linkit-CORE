#ifndef __MOCK_LOGGER_HPP_
#define __MOCK_LOGGER_HPP_

#include "logger.hpp"

class MockLog : public Logger {
public:

	void create() {
		mock().actualCall("create").onObject(this);
	}

	void write(void *entry) {
		mock().actualCall("create").onObject(this).withParameter("entry", entry);
	}

	void read(void *entry, int index=0) {
		mock().actualCall("create").onObject(this).withParameter("entry", entry).withParameter("index", index);
	}

	unsigned int num_entries() {
		return mock().actualCall("num_entries").returnIntValue();
	}
};

#endif // __MOCK_LOGGER_HPP_
