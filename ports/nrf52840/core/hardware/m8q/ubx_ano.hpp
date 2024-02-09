#pragma once

#include <cmath>
#include "filesystem.hpp"
#include "rtc.hpp"
#include "ubx.hpp"
#include "timeutils.hpp"
#include "debug.hpp"

extern RTC *rtc;

class UBXAssistNowOffline
{
private:
	File& m_file;
	uint8_t m_buffer[UBX::MAX_PACKET_LEN];
	lfs_soff_t m_end_pos;
	lfs_soff_t m_start_pos;
	bool       m_is_eof;

	bool read_next_ano_record() {

		while (1) {
			lfs_ssize_t sz = m_file.read(m_buffer, sizeof(UBX::Header));
			UBX::Header *ptr = (UBX::Header *)m_buffer;

			if (sz != sizeof(UBX::Header) ||
				ptr->syncChars[0] != UBX::SYNC_CHAR1 ||
				ptr->syncChars[1] != UBX::SYNC_CHAR2) {
				//DEBUG_TRACE("UBXAssistNowOfflineIterator:read_next_ano_record: sync error, sz=%d", sz);
				break;
			}

			if (ptr->msgClass != UBX::MessageClass::MSG_CLASS_MGA ||
				ptr->msgId != UBX::MGA::ID_ANO) {
				lfs_soff_t so = m_file.seek(ptr->msgLength + 2, LFS_SEEK_CUR);
				if (so <= 0) {
					//DEBUG_TRACE("UBXAssistNowOfflineIterator:read_next_ano_record: seek ANO error");
					break;
				}
				continue;
			}

			// Read payload
			sz = m_file.read(&m_buffer[sizeof(UBX::Header)], ptr->msgLength + 2);
			if (sz <= 0 || sz < (ptr->msgLength + 2)) {
				//DEBUG_TRACE("UBXAssistNowOfflineIterator:read_next_ano_record: read ANO error");
				break;
			}

			//DEBUG_TRACE("UBXAssistNowOfflineIterator:read_next_ano_record: read ANO @ %d", m_file.tell());
			return true;
		}

		return false;
	}

public:
	UBXAssistNowOffline(File& f) : m_file(f), m_end_pos(f.tell()), m_start_pos(f.tell()), m_is_eof(true) {
		std::time_t reftime = rtc->gettime();
		std::time_t deltatime = (std::time_t)0xFFFFFFFF;

		while (1) {

			// Record is read into m_buffer
			lfs_soff_t header_pos = m_file.tell();
			if (!read_next_ano_record()) {
				// Did not find another record
				//DEBUG_TRACE("UBXAssistNowOfflineIterator: no more records");
				break;
			}

			// Get pointer to ANO payload
			UBX::MGA::MSG_ANO *ano = (UBX::MGA::MSG_ANO *)&m_buffer[sizeof(UBX::Header)];

			// Convert date time to std::time_t using midday UTC as the reference time for each ANO message
			// Refer to Section 12.4.6 of u-blox receiver specification
			std::time_t anotime = convert_epochtime(2000 + ano->year, ano->month, ano->day, 12, 0, 0);
			std::time_t timediff = std::abs(anotime - reftime);
			//DEBUG_TRACE("UBXAssistNowOfflineIterator: dd/mm/yy=%02u/%02u/%02u t=%lld tdiff=%lld", (unsigned int)ano->day, (unsigned int)ano->month, (unsigned int)ano->year, anotime, timediff);

			if (timediff > deltatime) {
				// New sequence of ANO records with worse timestamp
				//DEBUG_TRACE("UBXAssistNowOfflineIterator: ANO sequence terminated @ %d", m_end_pos);
				break;
			} else if (timediff < deltatime) {
				// New sequence of ANO records with better timestamp
				//DEBUG_TRACE("UBXAssistNowOfflineIterator: ANO new sequence @ %d", header_pos);
				m_start_pos = header_pos;
				m_end_pos = m_file.tell();
				deltatime = timediff;
			} else {
				//DEBUG_TRACE("UBXAssistNowOfflineIterator: ANO new end pos @ %d", m_file.tell());
				m_end_pos = m_file.tell();
			}
		}

		if (m_end_pos > m_start_pos) {
			// Seek back to the first entry and read it into the buffer
			m_file.seek(m_start_pos);
			read_next_ano_record();
			m_is_eof = false;
		}
	}

	UBX::Header* value() {
		if (!m_is_eof)
		{
			return (UBX::Header *)m_buffer;
		} else {
			return nullptr;
		}
	}

	void next() {
		if (m_file.tell() < m_end_pos && read_next_ano_record())
			m_is_eof = false;
		else
			m_is_eof = true;
	}
};
