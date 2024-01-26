#pragma once

#include "filesystem.hpp"

#define IS25_BLOCK_COUNT   (4096)
#define IS25_BLOCK_SIZE    (4*1024)
#define IS25_PAGE_SIZE     (256)

class Is25Flash : public FlashInterface {
public:
	Is25Flash() : FlashInterface(IS25_BLOCK_COUNT, IS25_BLOCK_SIZE, IS25_PAGE_SIZE) { m_is_init = false; }
	void init();

private:
	bool m_is_init;
	int read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size) override;
	int prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) override;
	int erase(lfs_block_t block) override;
	int sync() override;

	int _read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size);
	int _prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
	int _erase(lfs_block_t block);
	int _sync();

	void power_up();
	void power_down();
};
