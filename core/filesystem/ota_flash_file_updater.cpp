#include "ota_flash_file_updater.hpp"
#include "error.hpp"
#include "crc32.hpp"
#include "dfu.hpp"


OTAFlashFileUpdater::OTAFlashFileUpdater(LFSFileSystem *filesystem, FlashInterface *flash_if, lfs_off_t reserved_block_offset, lfs_size_t reserved_blocks)
{
	m_filesystem = filesystem;
	m_flash_if = flash_if;
	m_reserved_block_offset = reserved_block_offset;
	m_reserved_blocks = reserved_blocks;
	m_file_size = 0;
}

OTAFlashFileUpdater::~OTAFlashFileUpdater()
{
	if (m_file_size && m_file_id != OTAFileIdentifier::MCU_FIRMWARE)
		delete m_file;
}

void OTAFlashFileUpdater::start_file_transfer(OTAFileIdentifier file_id, lfs_size_t length, uint32_t crc32) {

	if (m_file_size)
		throw ErrorCode::OTA_TRANSFER_ALREADY_IN_PROGRESS;

	if (length == 0 || length > (m_reserved_blocks * m_flash_if->m_block_size))
		throw ErrorCode::OTA_TRANSFER_BAD_FILE_SIZE;

	switch (file_id) {
	case OTAFileIdentifier::ARTIC_FIRMWARE:
		m_filesystem->remove("artic_firmware.dat");
		m_file = new LFSFile(m_filesystem, "artic_firmware.dat", LFS_O_WRONLY | LFS_O_CREAT);
		break;
	case OTAFileIdentifier::MCU_FIRMWARE:
		// Erase reserved region of external flash
		for (unsigned int i = 0; i < m_reserved_blocks; i++)
			m_flash_if->erase(i + m_reserved_block_offset);
		break;
	case OTAFileIdentifier::GPS_CONFIG:
		m_filesystem->remove("gps_config.dat");
		m_file = new LFSFile(m_filesystem, "gps_config.dat", LFS_O_WRONLY | LFS_O_CREAT);
		break;
	default:
		throw ErrorCode::OTA_TRANSFER_INVALID_FILE_ID;
		break;
	}
	m_file_id = file_id;
	m_file_size = length;
	m_crc32 = crc32;
	m_crc32_calc = 0xFFFFFFFF;
}

void OTAFlashFileUpdater::write_file_data(void * const data, lfs_size_t length)
{
	if (m_file_size == 0)
		throw ErrorCode::OTA_TRANSFER_NOT_STARTED;

	if (m_file_bytes_received + length > m_file_size) {
		abort_file_transfer();
		throw ErrorCode::OTA_TRANSFER_OVERFLOW;
	}

	if (m_file_id != OTAFileIdentifier::MCU_FIRMWARE)
		m_file->write(data, length);
	else
		m_flash_if->prog(m_file_bytes_received / m_flash_if->m_block_size, m_file_bytes_received % m_flash_if->m_block_size, data, length);

	m_file_bytes_received += length;

	CRC32::checksum((uint8_t *)data, length, m_crc32_calc);
}

void OTAFlashFileUpdater::abort_file_transfer()
{
	if (m_file_size != 0) {
		if (m_file_id != OTAFileIdentifier::MCU_FIRMWARE) {
			delete m_file;
		}
	}
	m_file_size = 0;
}

void OTAFlashFileUpdater::complete_file_transfer()
{
	if (m_file_size == 0)
		throw ErrorCode::OTA_TRANSFER_NOT_STARTED;

	if (m_file_bytes_received < m_file_size) {
		abort_file_transfer();
		throw ErrorCode::OTA_TRANSFER_INCOMPLETE;
	}

	if (m_crc32_calc != m_crc32) {
		abort_file_transfer();
		throw ErrorCode::OTA_TRANSFER_CRC_ERROR;
	}
}

void OTAFlashFileUpdater::apply_file_update() {
	if (m_file_id == OTAFileIdentifier::MCU_FIRMWARE) {
		// Update DFU settings page to take effect on the next reboot
		DFU::write_ext_flash_dfu_settings(m_reserved_block_offset, m_file_size, m_crc32);
	} else {
		delete m_file;
	}
	m_file_size = 0;
}
