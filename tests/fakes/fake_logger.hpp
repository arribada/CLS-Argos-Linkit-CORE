#ifndef __FAKE_LOGGER_HPP_
#define __FAKE_LOGGER_HPP_

#include <cstring>
#include "logger.hpp"
#include "messages.hpp"

class FakeLog : public Logger {
private:
	LogEntry m_entry[64];
	unsigned int m_index;

public:

	void create() {
		m_index = 0;
	}

	void write(void *entry) {
		std::memcpy(&m_entry[m_index++ % 64], entry, sizeof(LogEntry));
	}

	void read(void *entry, int index=0) {
		std::memcpy(entry, &m_entry[index], sizeof(LogEntry));
	}

	unsigned int num_entries() {
		return m_index;
	}
};

#endif // __FAKE_LOGGER_HPP_
