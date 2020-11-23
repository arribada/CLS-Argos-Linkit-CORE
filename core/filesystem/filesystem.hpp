#ifndef __FILESYSTEM_HPP_
#define __FILESYSTEM_HPP_

#include <iostream>
#include <cstring>

#include <stdint.h>
#include "lfs.h"

using namespace std;

class File;


class FileSystem {

private:
	lfs_t  m_lfs;

	// Subclasses must implement their own static methods for LittleFS read, prog, erase and sync

protected:
	struct lfs_config m_cfg;

public:
	FileSystem(unsigned int block_size, unsigned int blocks, unsigned int page_size) {
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
		m_cfg.read_buffer = new uint8_t[page_size]();
		m_cfg.prog_buffer = new uint8_t[page_size]();
		m_cfg.lookahead_buffer = new uint8_t[page_size]();

		// Attrs
		m_cfg.name_max = 32;
		m_cfg.attr_max = 4;
		m_cfg.file_max = 4*1024*1024;
	}

	~FileSystem() {
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

	friend class File;
};


class RamFileSystem : public FileSystem {
private:
	uint8_t *m_block_ram;

	static int read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size) {
		RamFileSystem *fs = reinterpret_cast<RamFileSystem*>(c->context);
		if (fs->m_debug_trace)
			std::cout << "read(" << block << " " << off << " " << size << ")\n";
		uint32_t offset = (block * fs->m_cfg.block_size) + off;
		std::memcpy(buffer, &fs->m_block_ram[offset], size);
		return 0;
	}
	static int prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
		RamFileSystem *fs = reinterpret_cast<RamFileSystem*>(c->context);
		if (fs->m_debug_trace)
			std::cout << "prog(" << block << " " << off << " " << size << ")\n";
		uint32_t offset = (block * fs->m_cfg.block_size) + off;
		std::memcpy(&fs->m_block_ram[offset], buffer, size);
		return 0;
	}
	static int erase(const struct lfs_config *c, lfs_block_t block)
	{
		RamFileSystem *fs = reinterpret_cast<RamFileSystem*>(c->context);
		if (fs->m_debug_trace)
			std::cout << "erase" << block << ")\n";
		uint32_t offset = (block * fs->m_cfg.block_size);
		std::memset(&fs->m_block_ram[offset], 0xFF, fs->m_cfg.block_size);
		return 0;
	}
	static int sync(const struct lfs_config *c) { return 0; }

public:
	bool m_debug_trace;
	RamFileSystem(unsigned int block_size, unsigned int blocks, unsigned int page_size) : FileSystem(block_size, blocks, page_size) {
		m_block_ram = new uint8_t[block_size * blocks]();
		m_cfg.read  = read;
		m_cfg.prog  = prog;
		m_cfg.erase = erase;
		m_cfg.sync  = sync;
		m_debug_trace = false;
	}
	~RamFileSystem() {
		delete[] m_block_ram;
	}
};

// File class wraps the commonly used per-file operations
class File {
protected:
	lfs_t      *m_lfs;
	lfs_file_t  m_file;

public:
	const char *m_path;

	File(FileSystem *fs, const char *path, int flags) {
		m_lfs = &fs->m_lfs;
		m_path = path;
		int ret = lfs_file_open(m_lfs, &m_file, path, flags);
		if (ret < 0)
			throw ret;
	}
	~File() {
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


// CircularFile is a subclass of File and will wrap its read/write operations at m_max_size.  It uses a persistent file
// attribute to keep track of the last write offset into the file i.e., m_offset.
class CircularFile : public File {

public:
	lfs_size_t  m_max_size;
	lfs_off_t   m_offset;
	int			m_flags;

	CircularFile(FileSystem *fs, const char *path, int flags, lfs_size_t max_size) : File(fs, path, flags) {
		m_max_size = max_size;
		m_offset = 0;
		m_flags = flags;
		if (flags & LFS_O_CREAT)
			lfs_setattr(m_lfs, m_path, 0, &m_offset, sizeof(m_offset));
		else
			lfs_getattr(m_lfs, m_path, 0, &m_offset, sizeof(m_offset));

		// Ensure we seek to the oldest entry in the file when file hasn't yet wrapped
		if (flags & LFS_O_RDONLY && size() < max_size)
			m_offset = 0;

		seek(m_offset);
	}

	~CircularFile() {
		if (m_flags & LFS_O_WRONLY)
			lfs_setattr(m_lfs, m_path, 0, &m_offset, sizeof(m_offset));
	}

	lfs_ssize_t read(void *buffer, lfs_size_t size) {
		int ret = File::read(buffer, size);
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
		int ret = File::write(buffer, size);
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
		return File::seek(m_offset);
	}
};

#endif // __FILESYSTEM_HPP_
