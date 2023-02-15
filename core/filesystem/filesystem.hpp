#ifndef __FILESYSTEM_HPP_
#define __FILESYSTEM_HPP_

#include <cstring>
#include <stdio.h>
#include <stdint.h>
#include "lfs.h"

class File;

class FileSystem {
public:
	virtual ~FileSystem() {}
	virtual int mount() = 0;
	virtual bool is_mounted() = 0;
	virtual int umount() = 0;
	virtual int format() = 0;
	virtual int remove(const char *path) = 0;
	virtual int stat(const char *path, struct lfs_info *info) = 0;
	virtual int set_attr(const char *path, unsigned int &attr) = 0;
	virtual int get_attr(const char *path, unsigned int &attr) = 0;
	virtual void *get_private_data() = 0;
};

class FlashInterface {
public:
	unsigned int m_blocks;
	unsigned int m_block_size;
	unsigned int m_page_size;

	FlashInterface(unsigned int block_size, unsigned int blocks, unsigned int page_size) {
		m_blocks = blocks;
		m_block_size = block_size;
		m_page_size = page_size;
	}
	virtual ~FlashInterface() {}

	virtual int read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size) = 0;
	virtual int prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) = 0;
	virtual int erase(lfs_block_t block) = 0;
	virtual int sync() = 0;
};


class LFSFileSystem : public FileSystem {
	friend class LFSFile;

private:
	struct lfs_config m_cfg;
	lfs_t  m_lfs;
	FlashInterface *m_flash_if;
	bool m_is_mounted;

	static int lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size) { return reinterpret_cast<LFSFileSystem*>(c->context)->m_flash_if->read(block, off, buffer, size); }
	static int lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) { return reinterpret_cast<LFSFileSystem*>(c->context)->m_flash_if->prog(block, off, buffer, size); }
	static int lfs_erase(const struct lfs_config *c, lfs_block_t block) { return reinterpret_cast<LFSFileSystem*>(c->context)->m_flash_if->erase(block); }
	static int lfs_sync(const struct lfs_config *c) { return reinterpret_cast<LFSFileSystem*>(c->context)->m_flash_if->sync(); }

public:
	LFSFileSystem(FlashInterface *flash_if, unsigned int blocks = 0) {
		m_cfg.context = static_cast<void*>(this);

		// Override blocks if zero
		if (0 == blocks)
			blocks = flash_if->m_blocks;

		// block device configuration
		m_cfg.read_size = flash_if->m_page_size;
		m_cfg.prog_size = flash_if->m_page_size;
		m_cfg.block_size = flash_if->m_block_size;
		m_cfg.block_count = blocks;
		m_cfg.cache_size = flash_if->m_page_size;
		m_cfg.lookahead_size = flash_if->m_page_size;
		m_cfg.block_cycles = 500;

		// Buffers
		m_cfg.read_buffer = new uint8_t[flash_if->m_page_size];
		m_cfg.prog_buffer = new uint8_t[flash_if->m_page_size];
		m_cfg.lookahead_buffer = new uint8_t[flash_if->m_page_size];

		// Attrs
		m_cfg.name_max = 32;
		m_cfg.attr_max = 4;
		m_cfg.file_max = 4*1024*1024;

		// Function pointers
		m_cfg.read  = lfs_read;
		m_cfg.prog  = lfs_prog;
		m_cfg.erase = lfs_erase;
		m_cfg.sync  = lfs_sync;

		// Flash interface
		m_flash_if = flash_if;

		// Mark as unmounted
		m_is_mounted = false;
	}

	virtual ~LFSFileSystem() {
		delete[] (uint8_t*)m_cfg.read_buffer;
		delete[] (uint8_t*)m_cfg.prog_buffer;
		delete[] (uint8_t*)m_cfg.lookahead_buffer;
	}

	int mount() override {
		int ret = lfs_mount(&m_lfs, &m_cfg);
		if (!ret)
			m_is_mounted = true;
		return ret;
	}

	int umount() override {
		m_is_mounted = false;
		return lfs_unmount(&m_lfs);
	}

	bool is_mounted() override {
		return m_is_mounted;
	}

	int format() override {
		m_is_mounted = false;
		return lfs_format(&m_lfs, &m_cfg);
	}

	int remove(const char *path) override {
		return lfs_remove(&m_lfs, path);
	}

	lfs_size_t get_block_size() {
		return m_cfg.block_size;
	}

	int stat(const char *path, struct lfs_info *info) override {
		return lfs_stat(&m_lfs, path, info);
	}

	int set_attr(const char *path, unsigned int &attr) override {
		return lfs_setattr(&m_lfs, path, 0, &attr, sizeof(attr));
	}

	int get_attr(const char *path, unsigned int &attr) override {
		return lfs_getattr(&m_lfs, path, 0, &attr, sizeof(attr));
	}

	void *get_private_data() override {
		return &m_lfs;
	}
};


class RamFlash : public FlashInterface {
private:
	uint8_t *m_block_ram;

public:
	RamFlash(unsigned int block_size, unsigned int blocks, unsigned int page_size) : FlashInterface(block_size, blocks, page_size) {
		m_block_ram = new uint8_t[block_size * blocks];
		m_debug_trace = false;
	}
	~RamFlash() {
		delete[] m_block_ram;
	}

	int read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size) override {
		if (m_debug_trace)
			printf("read(%lu %lu %lu)\n", block, off, size);
		uint32_t offset = (block * m_block_size) + off;
		std::memcpy(buffer, &m_block_ram[offset], size);
		return LFS_ERR_OK;
	}
	int prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) override {
		if (m_debug_trace)
			printf("prog(%lu %lu %lu)\n", block, off, size);
		uint32_t offset = (block * m_block_size) + off;
		std::memcpy(&m_block_ram[offset], buffer, size);
		return LFS_ERR_OK;
	}
	int erase(lfs_block_t block) override {
		if (m_debug_trace)
			printf("erase(%lu)\n", block);
		uint32_t offset = (block * m_block_size);
		std::memset(&m_block_ram[offset], 0xFF, m_block_size);
		return LFS_ERR_OK;
	}
	int sync() override { return LFS_ERR_OK; }

	bool m_debug_trace;
};

class File {
public:
	virtual lfs_ssize_t read(void *buffer, lfs_size_t size) = 0;
	virtual lfs_ssize_t write(void *buffer, lfs_size_t size) = 0;
	virtual lfs_soff_t seek(lfs_soff_t off, int whence=LFS_SEEK_SET) = 0;
	virtual lfs_soff_t tell() = 0;
	virtual int flush() = 0;
	virtual lfs_soff_t size() = 0;
};

// File class wraps the commonly used per-file operations
class LFSFile : public File {
protected:
	lfs_t      *m_lfs;
	lfs_file_t  m_file;
	const char *m_path;

public:
	LFSFile(FileSystem *fs, const char *path, int flags) {
		m_lfs = (lfs_t *)fs->get_private_data();
		m_path = path;
		int ret = lfs_file_open(m_lfs, &m_file, path, flags);
		if (ret < 0)
			throw ret;
	}
	virtual ~LFSFile() {
		lfs_file_close(m_lfs, &m_file);
	}
	lfs_ssize_t read(void *buffer, lfs_size_t size) override {
		return lfs_file_read(m_lfs, &m_file, buffer, size);
	}
	lfs_ssize_t write(void *buffer, lfs_size_t size) override {
		return lfs_file_write(m_lfs, &m_file, buffer, size);
	}
	lfs_soff_t seek(lfs_soff_t off, int whence=LFS_SEEK_SET) override {
		return lfs_file_seek(m_lfs, &m_file, off, whence);
	}
	lfs_soff_t tell() override {
		return lfs_file_tell(m_lfs, &m_file);
	}
	int flush() override {
		return lfs_file_sync(m_lfs, &m_file);
	}
	lfs_soff_t size() override {
		return lfs_file_size(m_lfs, &m_file);
	}
};


// LFSCircularFile is a subclass of LFSFile and will wrap its read/write operations at m_max_size.  It uses a persistent file
// attribute to keep track of the last write offset into the file i.e., m_offset.
class LFSCircularFile : public LFSFile {
private:
	lfs_size_t  m_max_size;
	lfs_off_t   m_offset;
	int			m_flags;

public:
	LFSCircularFile(FileSystem *fs, const char *path, int flags, lfs_size_t max_size) : LFSFile(fs, path, flags) {
		int ret;
		unsigned int attr = 0;
		m_max_size = max_size;
		m_flags = flags;
		if (flags & LFS_O_CREAT) {
			ret = fs->set_attr(m_path, attr);
		}
		else {
			ret = fs->get_attr(m_path, attr);
		}

		m_offset = attr;

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

	lfs_off_t get_offset() {
		return m_offset;
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

	lfs_soff_t tell() {
		return (lfs_soff_t)m_offset;
	}
};

#endif // __FILESYSTEM_HPP_
