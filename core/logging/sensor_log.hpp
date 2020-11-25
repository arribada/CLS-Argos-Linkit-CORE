#ifndef __SENSOR_LOG_HPP_
#define __SENSOR_LOG_HPP_

#include "logger.hpp"
#include "filesystem.hpp"

extern FileSystem *main_filesystem;

class SensorLog : public Logger {
public:
	static const unsigned int MAX_SIZE = 4 * 1024 * 1024;
	static constexpr const char *filename = "sensor.log";

	static void create() {
		CircularFile *f;
		try {
			// See if an existing file exists
			f = new CircularFile(main_filesystem, SensorLog::filename, LFS_O_RDONLY, SensorLog::MAX_SIZE);
		} catch (int e) {
			// Create a new log file if the file does not exist
			f = new CircularFile(main_filesystem, SensorLog::filename, LFS_O_WRONLY | LFS_O_CREAT, SensorLog::MAX_SIZE);
		}
		delete f;
	}

	static void write(GPSLogEntry *entry) {
		CircularFile f(main_filesystem, SensorLog::filename, LFS_O_WRONLY, SensorLog::MAX_SIZE);
		f.write(entry, (lfs_size_t)sizeof(*entry));
	}

	static void read(GPSLogEntry *entry, int index=0) {
		CircularFile f(main_filesystem, SensorLog::filename, LFS_O_RDONLY, SensorLog::MAX_SIZE);
		if (index != 0)
			f.seek(f.m_offset + index * (sizeof(*entry)));
		f.read(entry, (lfs_size_t)sizeof(*entry));
	}

	static unsigned int num_entries() {
		CircularFile f(main_filesystem, "sensor.log", LFS_O_RDONLY, SensorLog::MAX_SIZE);
		return f.size() / sizeof(GPSLogEntry);
	}
};

#endif // __SENSOR_LOG_HPP_
