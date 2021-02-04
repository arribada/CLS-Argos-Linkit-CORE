#ifndef __OTA_UPDATER_HPP_
#define __OTA_UPDATER_HPP_

#include "filesystem.hpp"

enum class OTAFileIdentifier {
	MCU_FIRMWARE,
	ARTIC_FIRMWARE,
	GPS_CONFIG
};

class OTAFileUpdater {
private:
	LFSFileSystem 	   *m_filesystem;
	FlashInterface 	   *m_flash_if;
	lfs_off_t      		m_reserved_block_offset;
	lfs_size_t     		m_reserved_blocks;
	OTAFileIdentifier   m_file_id;
	LFSFile            *m_file;
	lfs_size_t		    m_file_size;
	lfs_off_t		    m_file_bytes_received;
	unsigned int 		m_crc32;

public:
	OTAFileUpdater(LFSFileSystem *filesystem, FlashInterface *flash_if, lfs_off_t reserved_block_offset, lfs_size_t reserved_blocks);
	~OTAFileUpdater();
	void start_file_transfer(OTAFileIdentifier file_id, const lfs_size_t length, const unsigned int crc32);
	bool write_file_data(void * const data, lfs_size_t length);
	void abort_file_transfer();
	void complete_file_transfer();
	void apply_file_update();
};

#endif // __OTA_UPDATE_SERVICE_HPP_
