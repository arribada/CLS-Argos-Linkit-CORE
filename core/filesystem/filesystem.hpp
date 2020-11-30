#ifndef __FILESYSTEM_HPP_
#define __FILESYSTEM_HPP_

#include <iostream>
#include <cstring>

#include <stdint.h>
#include "lfs.h"

using namespace std;

class File;

class FileSystem {
public:
	virtual int mount() = 0;
	virtual int umount() = 0;
	virtual int format() = 0;
};

class LFSFileSystem : public FileSystem {
	friend class LFSFile;

private:
	struct lfs_config m_cfg;
	lfs_t  m_lfs;

	static int lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size) { return reinterpret_cast<LFSFileSystem*>(c->context)->read(block, off, buffer, size); }
	static int lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) { return reinterpret_cast<LFSFileSystem*>(c->context)->prog(block, off, buffer, size); }
	static int lfs_erase(const struct lfs_config *c, lfs_block_t block) { return reinterpret_cast<LFSFileSystem*>(c->context)->erase(block); }
	static int lfs_sync(const struct lfs_config *c) { return reinterpret_cast<LFSFileSystem*>(c->context)->sync(); }

	// Derived classes must implement their own methods for LittleFS read, prog, erase and sync

	virtual int read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size) = 0;
	virtual int prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) = 0;
	virtual int erase(lfs_block_t block) = 0;
	virtual int sync() = 0;

public:
	LFSFileSystem(unsigned int block_size, unsigned int blocks, unsigned int page_size) {
		m_cfg.context = static_cast<void*>(this);

		// block device configuration
		m_cfg.read_size = page_size;
		m_cfg.prog_size = page_size;
		m_cfg.block_size = block_size;
		m_cfg.block_count = blocks;
		m_cfg.cache_size = page_size;
		m_cfg.lookahead_size = page_size;
		m_cfg.block_cycles = 500;

		// Buffers
		m_cfg.read_buffer = new uint8_t[page_size];
		m_cfg.prog_buffer = new uint8_t[page_size];
		m_cfg.lookahead_buffer = new uint8_t[page_size];

		// Attrs
		m_cfg.name_max = 32;
		m_cfg.attr_max = 4;
		m_cfg.file_max = 4*1024*1024;

		// Function pointers
		m_cfg.read  = lfs_read;
		m_cfg.prog  = lfs_prog;
		m_cfg.erase = lfs_erase;
		m_cfg.sync  = lfs_sync;
	}

	~LFSFileSystem() {
		delete[] (uint8_t*)m_cfg.read_buffer;
		delete[] (uint8_t*)m_cfg.prog_buffer;
		delete[] (uint8_t*)m_cfg.lookahead_buffer;
	}

	int mount() {
		return lfs_mount(&m_lfs, &m_cfg);
	}

	int umount() {
		return lfs_unmount(&m_lfs);
	}

	int format() {
		return lfs_format(&m_lfs, &m_cfg);
	}

	lfs_size_t get_block_size() {
		return m_cfg.block_size;
	}
};


class LFSRamFileSystem : public LFSFileSystem {
private:
	uint8_t *m_block_ram;

	int read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size) override {
		if (m_debug_trace)
			std::cout << "read(" << block << " " << off << " " << size << ")\n";
		uint32_t offset = (block * get_block_size()) + off;
		std::memcpy(buffer, &m_block_ram[offset], size);
		return LFS_ERR_OK;
	}
	int prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) override {
		if (m_debug_trace)
			std::cout << "prog(" << block << " " << off << " " << size << ")\n";
		uint32_t offset = (block * get_block_size()) + off;
		std::memcpy(&m_block_ram[offset], buffer, size);
		return LFS_ERR_OK;
	}
	int erase(lfs_block_t block) override {
		if (m_debug_trace)
			std::cout << "erase" << block << ")\n";
		uint32_t offset = (block * get_block_size());
		std::memset(&m_block_ram[offset], 0xFF, get_block_size());
		return LFS_ERR_OK;
	}
	int sync() override { return LFS_ERR_OK; }

public:
	bool m_debug_trace;
	LFSRamFileSystem(unsigned int block_size, unsigned int blocks, unsigned int page_size) : LFSFileSystem(block_size, blocks, page_size) {
		m_block_ram = new uint8_t[block_size * blocks];
		m_debug_trace = false;
	}
	~LFSRamFileSystem() {
		delete[] m_block_ram;
	}
};

class File {
public:
	virtual lfs_ssize_t read(void *buffer, lfs_size_t size) = 0;
	virtual lfs_ssize_t write(void *buffer, lfs_size_t size) = 0;
	virtual lfs_soff_t seek(lfs_soff_t off, int whence=LFS_SEEK_SET) = 0;
	virtual int flush() = 0;
	virtual lfs_soff_t size() = 0;
};

// File class wraps the commonly used per-file operations
class LFSFile : public File {
protected:
	lfs_t      *m_lfs;
	lfs_file_t  m_file;

public:
	const char *m_path;

	LFSFile(LFSFileSystem *fs, const char *path, int flags) {
		m_lfs = &fs->m_lfs;
		m_path = path;
		int ret = lfs_file_open(m_lfs, &m_file, path, flags);
		if (ret < 0)
			throw ret;
	}
	~LFSFile() {
		lfs_file_close(m_lfs, &m_file);
	}
	lfs_ssize_t read(void *buffer, lfs_size_t size) {
		return lfs_file_read(m_lfs, &m_file, buffer, size);
	}
	lfs_ssize_t write(void *buffer, lfs_size_t size) {
		return lfs_file_write(m_lfs, &m_file, buffer, size);
	}
	lfs_soff_t seek(lfs_soff_t off, int whence=LFS_SEEK_SET) {
		return lfs_file_seek(m_lfs, &m_file, off, whence);
	}
	int flush() {
		return lfs_file_sync(m_lfs, &m_file);
	}
	lfs_soff_t size() {
		return lfs_file_size(m_lfs, &m_file);
	}
};


// LFSCircularFile is a subclass of LFSFile and will wrap its read/write operations at m_max_size.  It uses a persistent file
// attribute to keep track of the last write offset into the file i.e., m_offset.
class LFSCircularFile : public LFSFile {

public:
	lfs_size_t  m_max_size;
	lfs_off_t   m_offset;
	int			m_flags;

	LFSCircularFile(LFSFileSystem *fs, const char *path, int flags, lfs_size_t max_size) : LFSFile(fs, path, flags) {
		int ret;
		m_max_size = max_size;
		m_offset = 0;
		m_flags = flags;
		if (flags & LFS_O_CREAT)
			ret = lfs_setattr(m_lfs, m_path, 0, &m_offset, sizeof(m_offset));
		else
			ret = lfs_getattr(m_lfs, m_path, 0, &m_offset, sizeof(m_offset));
		
		if (ret < 0)
			throw ret;
		
		lfs_soff_t file_size = size();
		if (file_size < 0)
			throw file_size;

		// Ensure we seek to the oldest entry in the file when file hasn't yet wrapped
		if (flags & LFS_O_RDONLY && static_cast<lfs_size_t>(file_size) < max_size)
			m_offset = 0;

		seek(m_offset);
	}

	~LFSCircularFile() {
		if (m_flags & LFS_O_WRONLY)
			lfs_setattr(m_lfs, m_path, 0, &m_offset, sizeof(m_offset));
	}

	lfs_ssize_t read(void *buffer, lfs_size_t size) {
		int ret = LFSFile::read(buffer, size);
		if (ret >= 0)
		{
			m_offset += ret;
			if (m_offset >= m_max_size)
			{
				m_offset = m_offset % m_max_size;
				seek(m_offset);
			}
		}
		return ret;
	}

	lfs_ssize_t write(void *buffer, lfs_size_t size) {
		int ret = LFSFile::write(buffer, size);
		if (ret >= 0)
		{
			m_offset += ret;
			if (m_offset >= m_max_size)
			{
				m_offset = m_offset % m_max_size;
				seek(m_offset);
			}
		}
		return ret;
	}

	lfs_soff_t seek(lfs_soff_t offset) {
		m_offset = offset % m_max_size;
		return LFSFile::seek(m_offset);
	}
};

#endif // __FILESYSTEM_HPP_
