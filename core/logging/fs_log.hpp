#ifndef __FS_LOG_HPP_
#define __FS_LOG_HPP_

#include "filesystem.hpp"
#include "logger.hpp"

class FsLog : public Logger {

private:
	LFSFileSystem *m_filesystem;
	unsigned int m_max_size;
	const char *m_filename;

	void set_payload_size(LogEntry *entry) {
		if (entry->header.log_type == LogType::LOG_GPS)
			entry->header.payload_size = sizeof(GPSInfo);
	}
public:

	FsLog(LFSFileSystem *fs, const char *filename, const unsigned int max_size) {
		m_filesystem = fs;
		m_filename = filename;
		m_max_size = max_size;
	}

	bool is_ready() override { return m_filesystem->is_mounted(); }

	void create() {
		try {
			// See if an existing file exists
			LFSCircularFile f(m_filesystem, m_filename, LFS_O_RDONLY, m_max_size);
		} catch (int e) {
			// Create a new log file if the file does not exist
			LFSCircularFile f(m_filesystem, m_filename, LFS_O_WRONLY | LFS_O_CREAT, m_max_size);
		}
	}

	void write(void *entry) {
		LFSCircularFile f(m_filesystem, m_filename, LFS_O_WRONLY, m_max_size);
		set_payload_size((LogEntry *)entry);
		f.write(entry, (lfs_size_t)sizeof(LogEntry));
	}

	void read(void *entry, int index=0) {
		LFSCircularFile f(m_filesystem, m_filename, LFS_O_RDONLY, m_max_size);
		if (index != 0)
			f.seek(f.get_offset() + index * (sizeof(LogEntry)));
		f.read(entry, (lfs_size_t)sizeof(LogEntry));
	}

	unsigned int num_entries() {
		LFSCircularFile f(m_filesystem, m_filename, LFS_O_RDONLY, m_max_size);
		return f.size() / sizeof(LogEntry);
	}
};

#endif // __FS_LOG_HPP_
