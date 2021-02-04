#include "ota_file_updater.hpp"

OTAFileUpdater::OTAFileUpdater(LFSFileSystem *filesystem, FlashInterface *flash_if, lfs_off_t reserved_block_offset, lfs_size_t reserved_blocks)
{
	m_filesystem = filesystem;
	m_flash_if = flash_if;
	m_reserved_block_offset = reserved_block_offset;
	m_reserved_blocks = reserved_blocks;
}

OTAFileUpdater::~OTAFileUpdater()
{
}

void OTAFileUpdater::start_file_transfer(OTAFileIdentifier file_id, lfs_size_t length, unsigned int crc32) {
	(void)file_id;
	(void)length;
	(void)crc32;
}

bool OTAFileUpdater::write_file_data(void * const data, lfs_size_t length) {
	(void)data;
	(void)length;
	return false;
}

void OTAFileUpdater::abort_file_transfer() {}
void OTAFileUpdater::complete_file_transfer() {}
void OTAFileUpdater::apply_file_update() {}
