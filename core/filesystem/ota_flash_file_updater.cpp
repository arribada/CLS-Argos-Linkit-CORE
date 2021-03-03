#include <iostream>
#include "ota_flash_file_updater.hpp"
#include "error.hpp"
#include "crc32.hpp"
#include "debug.hpp"

// Flash header for firmware image in external flash
static constexpr const uint32_t FLASH_HEADER_SIZE = sizeof(lfs_size_t) + sizeof(uint32_t);


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

	if (m_file_size) {
		DEBUG_ERROR("OTAFlashFileUpdater::start_file_transfer: transfer already in progress");
		throw ErrorCode::OTA_TRANSFER_ALREADY_IN_PROGRESS;
	}

	if (length == 0 || length > (m_reserved_blocks * m_flash_if->m_block_size) - FLASH_HEADER_SIZE) {
		DEBUG_ERROR("OTAFlashFileUpdater::start_file_transfer: bad transfer size %u bytes", length);
		throw ErrorCode::OTA_TRANSFER_BAD_FILE_SIZE;
	}

	switch (file_id) {
	case OTAFileIdentifier::ARTIC_FIRMWARE:
		DEBUG_INFO("OTAFlashFileUpdater::start_file_transfer: ARTIC_FIRMWARE");
		m_filesystem->remove("artic_firmware.dat");
		m_file = new LFSFile(m_filesystem, "artic_firmware.dat", LFS_O_WRONLY | LFS_O_CREAT);
		break;
	case OTAFileIdentifier::MCU_FIRMWARE:
		DEBUG_INFO("OTAFlashFileUpdater::start_file_transfer: MCU_FIRMWARE");
		// Erase reserved region of external flash
		for (unsigned int i = 0; i < m_reserved_blocks; i++) {
			uint8_t buffer[256];
			m_flash_if->read(m_reserved_block_offset + i, 0, buffer, 256);
			for (unsigned int j = 0; j < 256; j++) {
				if (buffer[j] != 0xFF) {
					m_flash_if->erase(i + m_reserved_block_offset);
					break;
				}
			}
		}

		// Write the header information into the start of flash
		m_flash_if->prog(m_reserved_block_offset, 0, &length, sizeof(length));
		m_flash_if->sync();
		m_flash_if->prog(m_reserved_block_offset, sizeof(length), &crc32, sizeof(crc32));
		m_flash_if->sync();
		break;
	case OTAFileIdentifier::GPS_CONFIG:
		DEBUG_INFO("OTAFlashFileUpdater::start_file_transfer: GPS_CONFIG");
		m_filesystem->remove("gps_config.dat");
		m_file = new LFSFile(m_filesystem, "gps_config.dat", LFS_O_WRONLY | LFS_O_CREAT);
		break;
	default:
		throw ErrorCode::OTA_TRANSFER_INVALID_FILE_ID;
		break;
	}
	DEBUG_INFO("OTAFlashFileUpdater::start_file_transfer: m_file_id=%u, m_file_size=%u crc32=%08x",
			   (unsigned int)file_id, length, crc32);
	m_file_id = file_id;
	m_file_size = length;
	m_crc32 = crc32;
	m_crc32_calc = 0;
	m_file_bytes_received = 0;
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
	{
		std::vector<uint8_t> aligned_buffer;
		aligned_buffer.resize(length);
		std::memcpy(&aligned_buffer[0], data, length);
		m_flash_if->prog(0, (m_reserved_block_offset * m_flash_if->m_block_size) + m_file_bytes_received + FLASH_HEADER_SIZE, &aligned_buffer[0], length);
		m_flash_if->sync();
	}

	m_file_bytes_received += length;

	DEBUG_TRACE("OTAFlashFileUpdater::write_file_data: %u/%u", m_file_bytes_received, m_file_size);

	m_crc32_calc ^= 0xFFFFFFFF;
	CRC32::checksum((unsigned char *)data, length, m_crc32_calc);
}

void OTAFlashFileUpdater::abort_file_transfer()
{
	if (m_file_size != 0) {
		if (m_file_id != OTAFileIdentifier::MCU_FIRMWARE) {
			delete m_file;
		}
		else
		{
			// Erase the first block to ensure firmware update header is erased
			m_flash_if->erase(m_reserved_block_offset);
		}
	}
	m_file_size = 0;
}

void OTAFlashFileUpdater::complete_file_transfer()
{
	if (m_file_size == 0)
		throw ErrorCode::OTA_TRANSFER_NOT_STARTED;

	if (m_file_bytes_received < m_file_size) {
		DEBUG_ERROR("OTAFlashFileUpdater:: not all bytes received");
		abort_file_transfer();
		throw ErrorCode::OTA_TRANSFER_INCOMPLETE;
	}

	if (m_crc32_calc != m_crc32) {
		DEBUG_ERROR("OTAFlashFileUpdater:: CRC failure");
		abort_file_transfer();
		throw ErrorCode::OTA_TRANSFER_CRC_ERROR;
	}
}

void OTAFlashFileUpdater::apply_file_update() {
	DEBUG_TRACE("OTAFlashFileUpdater::apply_file_update");
	if (m_file_id == OTAFileIdentifier::MCU_FIRMWARE) {
		DEBUG_INFO("OTAFlashFileUpdater::apply_file_update: device reset required for update to take effect");
	} else {
		delete m_file;
	}
	m_file_size = 0;
}
