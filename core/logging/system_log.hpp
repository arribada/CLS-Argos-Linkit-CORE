#ifndef __SYSTEM_LOG_HPP_
#define __SYSTEM_LOG_HPP_

#include "logger.hpp"
#include "filesystem.hpp"

extern FileSystem *main_filesystem;

class SystemLog : public Logger {
public:
	static const unsigned int MAX_SIZE = 4 * 1024 * 1024;
	static const char *filename = "system.log";

	static void create() {
		CircularFile *f;
		try {
			// See if an existing file exists
			f = new CircularFile(main_filesystem, SystemLog::filename, LFS_O_RDONLY, SystemLog::MAX_SIZE);
		} catch (int e) {
			// Create a new log file if the file does not exist
			f = new CircularFile(main_filesystem, SystemLog::filename, LFS_O_WRONLY | LFS_O_CREAT, SystemLog::MAX_SIZE);
		}
		delete f;
	}

	static void write(void *entry) {
		CircularFile f(main_filesystem, SystemLog::filename, LFS_O_WRONLY, SystemLog::MAX_SIZE);
		f.write(entry, (lfs_size_t)sizeof(LogEntry));
	}

	static void read(void *entry, int index=0) {
		CircularFile f(main_filesystem, SystemLog::filename, LFS_O_RDONLY, SystemLog::MAX_SIZE);
		if (index != 0)
			f.seek(f.m_offset + index * (sizeof(LogEntry)));
		f.read(entry, (lfs_size_t)sizeof(LogEntry));
	}

	static unsigned int num_entries() {
		CircularFile f(main_filesystem, "sensor.log", LFS_O_RDONLY, SystemLog::MAX_SIZE);
		return f.size() / sizeof(LogEntry);
	}
};

#endif // __SYSTEM_LOG_HPP_
