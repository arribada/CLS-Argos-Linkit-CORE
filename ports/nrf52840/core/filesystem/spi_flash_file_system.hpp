#pragma once

#include "filesystem.hpp"

class LFSSpiFlashFileSystem : public LFSFileSystem {
public:
	LFSSpiFlashFileSystem(unsigned int block_size, unsigned int blocks, unsigned int page_size) : LFSFileSystem(block_size, blocks, page_size) {}
	void init();

private:
	int read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size) override;
	int prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) override;
	int erase(lfs_block_t block) override;
	int sync() override;
};