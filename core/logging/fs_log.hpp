#ifndef __FS_LOG_HPP_
#define __FS_LOG_HPP_

#include <string>
#include "filesystem.hpp"
#include "logger.hpp"
#include "debug.hpp"
#include "error.hpp"

// Size of individual log chunks -- a separate file is used per chunk of the overall log
#define LOG_CHUNK_SIZE				(4*1024)


class FsLog : public Logger {

private:
	LFSFileSystem *m_filesystem;
	LFSFile *m_file_read;				// Used for avoiding file open overhead on reads
	LFSFile *m_file_write;				// Used for avoiding file open overhead on writes
	unsigned int m_last_read_index;		// Last file index read
	unsigned int m_max_size;			// Maximum overall log size
	unsigned int m_write_offset;		// Current write offset written as file attribute
	const char *m_filename;				// Base name of log file
	unsigned int m_has_wrapped;			// Tracks if the log file has wrapped
	bool m_is_ready;

	void set_payload_size(LogEntry *entry) {
		if (entry->header.log_type == LogType::LOG_GPS)
			entry->header.payload_size = sizeof(GPSInfo);
	}
public:

	FsLog(LFSFileSystem *fs, const char *filename, const unsigned int max_size) {
		m_filesystem = fs;
		m_filename = filename;
		m_max_size = max_size;
		m_write_offset = 0;
		m_has_wrapped = 0;
		m_file_read = nullptr;
		m_file_write = nullptr;
		m_last_read_index = (unsigned int)-1;
		m_is_ready = false;
	}

	~FsLog() {
		if (m_file_read)
			delete m_file_read;
	}

	bool is_ready() override { return m_is_ready; }

	void create() {
		try {
			// See if log file exists
			{
				LFSFile f(m_filesystem, m_filename, LFS_O_RDONLY);
			}

			// Get the current attribute from the file
			unsigned int attr;
			m_filesystem->get_attr(m_filename, attr);
			m_write_offset = attr & 0x7FFFFFFF;
			m_has_wrapped = attr & 0x80000000;
			//DEBUG_TRACE("FsLog::create: file=%s already exists with attr=%08x", m_filename, attr);

			// Check that the file attribute is valid and in range
			if (m_write_offset >= m_max_size) {
				DEBUG_ERROR("FsLog::create: illegal file attribute value detected - %08x", attr);
				throw ErrorCode::BAD_FILESYSTEM;
			}

			// Check existence of all required log file chunks
			for (unsigned int i = 0; i <= (m_has_wrapped ? m_max_size-1 : m_write_offset) / LOG_CHUNK_SIZE; i++) {
				unsigned int file_index = i;
				std::string filename(m_filename);
				filename += "." + std::to_string(file_index);
				try
				{
					LFSFile f(m_filesystem, filename.c_str(), LFS_O_RDONLY);
				} catch (int e) {
					DEBUG_ERROR("FsLog::create: missing log chunk %u", file_index);
					throw ErrorCode::BAD_FILESYSTEM;
				}
			}

		} catch (int e) {
			// Create a new log file if the file does not exist
			{
				LFSFile f(m_filesystem, m_filename, LFS_O_WRONLY | LFS_O_CREAT);
			}
			unsigned int attr = m_write_offset | m_has_wrapped;
			m_filesystem->set_attr(m_filename, attr);
			//DEBUG_TRACE("FsLog::create: file=%s does not exist setting attr=%08x", m_filename, attr);

			// Create an empty first log chunk
			unsigned int file_index = 0;
			std::string filename(m_filename);
			filename += "." + std::to_string(file_index);
			{
				LFSFile f(m_filesystem, filename.c_str(), LFS_O_WRONLY | LFS_O_CREAT);
			}

			DEBUG_INFO("FsLog::create: new log file %s created", m_filename);
		}

		m_is_ready = true;
	}

	void write(void *entry) {
		if (!m_is_ready)
			throw ErrorCode::RESOURCE_NOT_AVAILABLE;

		// To minimize the risk of data loss, this function will always open/close the file
		// object each time and also write to the attributes file to update the write offset
		// and log wrap flag

		// Construct file index for log chunk
		unsigned int file_index = (m_write_offset / LOG_CHUNK_SIZE);
		std::string filename(m_filename);
		filename += "." + std::to_string(file_index);
		int flags;
		if ((m_write_offset % LOG_CHUNK_SIZE) == 0) {
			// File should be forcibly created on each log chunk boundary
			flags = LFS_O_CREAT | LFS_O_WRONLY;
		} else {
			// Log chunk should be opened normally as it has already been created
			flags = LFS_O_WRONLY;
		}

		//DEBUG_TRACE("FsLog::write: chunk=%u offset=%u", file_index, m_write_offset);
		LFSFile f(m_filesystem, filename.c_str(), flags);
		set_payload_size((LogEntry *)entry);

		// Note that we always do a seek before writing -- this is a safety measure
		// just in case there was a power loss between writing the file and the
		// file attribute.  It assures we don't get log chunks that could exceed the
		// log chunk size.
		f.seek(m_write_offset % LOG_CHUNK_SIZE);  // Should be the end of the file
		f.write(entry, (lfs_size_t)sizeof(LogEntry));

		// Update write index, wrapped status and write the file attribute
		m_write_offset += sizeof(LogEntry);
		if (m_write_offset >= m_max_size) {
			m_write_offset = 0;
			m_has_wrapped = 0x80000000;
		}
		unsigned int attr = m_write_offset | m_has_wrapped;
		m_filesystem->set_attr(m_filename, attr);

		// If we have just written into the current read file index then delete the file read object
		// to force a reopen on the next read -- this ensures that any new data will be picked up by
		// the file object
		if (file_index == m_last_read_index) {
			delete m_file_read;
			m_file_read = nullptr;
			m_last_read_index = (unsigned int)-1;
		}
	}

	void read(void *entry, int index=0) {

		if (!m_is_ready)
			throw ErrorCode::RESOURCE_NOT_AVAILABLE;

		// Compute the log chunk we need to read
		unsigned int read_offset = 0;
		if (m_has_wrapped) {
			// This calculation uses the write offset rounded up to the nearest log chunk boundary as
			// the base read position of the log file.  Note that the rounding up is necessary since
			// when overwriting previous log chunks they get deleted first.
			read_offset = ((m_write_offset + (LOG_CHUNK_SIZE - 1)) / LOG_CHUNK_SIZE) * LOG_CHUNK_SIZE;
		}

		// Construct file index for log chunk to read
		lfs_soff_t file_offset = (read_offset + (index * sizeof(LogEntry))) % m_max_size;
		unsigned int file_index = (file_offset / LOG_CHUNK_SIZE);

		//DEBUG_TRACE("FsLog::read: chunk=%u file_offset=%u m_max_size=%u read_base=%u index=%u woffset=%u", file_index, file_offset, m_max_size, read_offset, index, m_write_offset);

		// Check if the cached read file handle is pointing to the correct file for reading
		if (m_last_read_index != file_index) {
			// Close the existing cached file handle (if any) and open a new one
			std::string filename(m_filename);
			filename += "." + std::to_string(file_index);
			if (m_file_read)
				delete m_file_read;
			m_file_read = new LFSFile(m_filesystem, filename.c_str(), LFS_O_RDONLY);
			m_last_read_index = file_index;
		}

		// Seek to desired position in file and read the log entry out of the log chunk
		m_file_read->seek(file_offset % LOG_CHUNK_SIZE);
		m_file_read->read(entry, sizeof(LogEntry));
	}

	unsigned int num_entries() {
		if (m_has_wrapped) {
			// This calculation uses the write offset rounded up to the nearest log chunk boundary as
			// the base read position of the log file.  Note that the rounding up is necessary since
			// when overwriting previous log chunks they get deleted first.
			if ((m_write_offset % LOG_CHUNK_SIZE) == 0)
				return m_max_size / sizeof(LogEntry); // Full log file if write offset is on log chunk boundary
			else
				return (m_max_size - (LOG_CHUNK_SIZE - (m_write_offset % LOG_CHUNK_SIZE))) / sizeof(LogEntry);
		} else {
			// When we are yet to wrap then the number of entries is simply derived from the current write offset
			return m_write_offset / sizeof(LogEntry);
		}
	}
};

#endif // __FS_LOG_HPP_
