#pragma once

#include <cstring>
#include "logger.hpp"
#include "messages.hpp"

class FakeLog : public Logger {
private:
	LogEntry m_entry[2048];
	unsigned int m_index;

public:

	FakeLog(const char *name = "Fake") : Logger(name) {}

	void create() {
		m_index = 0;
	}

	void truncate() {
		m_index = 0;
	}

	bool is_ready() {
		return true;
	}

	void write(void *entry) {
		std::memcpy(&m_entry[m_index++ % 2048], entry, sizeof(LogEntry));
	}

	void read(void *entry, int index=0) {
		std::memcpy(entry, &m_entry[index], sizeof(LogEntry));
	}

	unsigned int num_entries() {
		return m_index;
	}
};
