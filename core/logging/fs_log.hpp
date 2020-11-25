#ifndef __FS_LOG_HPP_
#define __FS_LOG_HPP_

#include "filesystem.hpp"
#include "logger.hpp"

class FsLog : public Logger {

private:
	LFSFileSystem *m_filesystem;
	const unsigned int m_max_size;
	const char *m_filename;

public:

	FsLog(LFSFileSystem *fs, const char *filename, const unsigned int max_size) {
		m_filesystem = fs;
		m_filename = filename;
		m_max_size = max_size;
	}

	void create() {
		LFSCircularFile *f;
		try {
			// See if an existing file exists
			f = new LFSCircularFile(m_filesystem, m_filename, LFS_O_RDONLY, m_max_size);
		} catch (int e) {
			// Create a new log file if the file does not exist
			f = new LFSCircularFile(m_filesystem, m_filename, LFS_O_WRONLY | LFS_O_CREAT, m_max_size);
		}
		delete f;
	}

	void write(void *entry) {
		LFSCircularFile f(m_filesystem, m_filename, LFS_O_WRONLY, m_max_size);
		f.write(entry, (lfs_size_t)sizeof(LogEntry));
	}

	void read(void *entry, int index=0) {
		LFSCircularFile f(m_filesystem, m_filename, LFS_O_RDONLY, m_max_size);
		if (index != 0)
			f.seek(f.m_offset + index * (sizeof(LogEntry)));
		f.read(entry, (lfs_size_t)sizeof(LogEntry));
	}

	unsigned int num_entries() {
		LFSCircularFile f(m_filesystem, "sensor.log", LFS_O_RDONLY, m_max_size);
		return f.size() / sizeof(LogEntry);
	}
};

#endif // __FS_LOG_HPP_
