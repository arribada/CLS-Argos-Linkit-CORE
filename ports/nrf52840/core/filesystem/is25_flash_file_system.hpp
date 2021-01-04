#pragma once

#include "filesystem.hpp"

#define IS25_BLOCK_COUNT   (4096)
#define IS25_BLOCK_SIZE    (4*1024)
#define IS25_PAGE_SIZE     (256)

class Is25FlashFileSystem : public LFSFileSystem {
public:
	Is25FlashFileSystem() : LFSFileSystem(IS25_BLOCK_COUNT, IS25_BLOCK_SIZE, IS25_PAGE_SIZE) {}
	void init();

private:
	int read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size) override;
	int prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) override;
	int erase(lfs_block_t block) override;
	int sync() override;
};