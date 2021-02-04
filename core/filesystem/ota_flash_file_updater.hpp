#pragma once

#include <stdint.h>
#include "ota_file_updater.hpp"
#include "filesystem.hpp"


class OTAFlashFileUpdater : public OTAFileUpdater {
private:
	LFSFileSystem 	   *m_filesystem;
	FlashInterface 	   *m_flash_if;
	lfs_off_t      		m_reserved_block_offset;
	lfs_size_t     		m_reserved_blocks;
	OTAFileIdentifier   m_file_id;
	LFSFile            *m_file;
	lfs_size_t		    m_file_size;
	lfs_off_t		    m_file_bytes_received;
	uint32_t			m_crc32;
	uint32_t			m_crc32_calc;

public:
	OTAFlashFileUpdater(LFSFileSystem *filesystem, FlashInterface *flash_if, lfs_off_t reserved_block_offset, lfs_size_t reserved_blocks);
	~OTAFlashFileUpdater();
	void start_file_transfer(OTAFileIdentifier file_id, const lfs_size_t length, const uint32_t crc32) override;
	void write_file_data(void * const data, lfs_size_t length) override;
	void abort_file_transfer() override;
	void complete_file_transfer() override;
	void apply_file_update() override;
};
