#include <cstring>
#include "ubx_comms.hpp"
#include "nrf_libuarte_async.h"
#include "bsp.hpp"
#include "error.hpp"
#include "debug.hpp"
#include "binascii.hpp"
#include "interrupt_lock.hpp"

using namespace UBX;

class AsyncLowLevel {
public:
	static void nrf_libuarte_async_evt_handler(void * context, nrf_libuarte_async_evt_t * p_evt) {
		UBXComms *obj = (UBXComms *)context;
		if (p_evt->type == NRF_LIBUARTE_ASYNC_EVT_TX_DONE) {
			obj->handle_tx_done();
		} else if (p_evt->type == NRF_LIBUARTE_ASYNC_EVT_RX_DATA) {
			obj->handle_rx_buffer(p_evt->data.rxtx.p_data, (unsigned int)p_evt->data.rxtx.length);
		} else if (p_evt->type == NRF_LIBUARTE_ASYNC_EVT_ERROR) {
			// Stop the receiver to prevent further errors
			obj->handle_error((unsigned int)p_evt->data.errorsrc);
        } else if (p_evt->type == NRF_LIBUARTE_ASYNC_EVT_ALLOC_ERROR) {
            // Stop the receiver to prevent further errors
            obj->handle_error(0x100);
		} else if (p_evt->type == NRF_LIBUARTE_ASYNC_EVT_OVERRUN_ERROR) {
            // Stop the receiver to prevent further errors
            obj->handle_error(0x200);
		}
	}
};

UBXComms::UBXComms(unsigned int instance) : m_instance(instance), m_is_init(false) {
}

void UBXComms::init() {
    if (m_is_init) return;
    if (nrf_libuarte_async_init(
            BSP::UARTAsync_Inits[m_instance].uart,
            &BSP::UARTAsync_Inits[m_instance].config,
            AsyncLowLevel::nrf_libuarte_async_evt_handler,
            (void *)this) != NRF_SUCCESS) {
        DEBUG_ERROR("UBXComms::UBXComms: failed to initialize libuarte library");
        throw ErrorCode::RESOURCE_NOT_AVAILABLE;
    }
    m_is_send_busy = false;
    m_nav_report_iTOW = -1;
    m_dbd_filter_en = false;
    m_expect.enable = false;
    m_is_init = true;
    m_debug_enable = false;
    m_nav_report.pvt.iTow = -1;
    m_nav_report.dop.iTow = -1;
    m_nav_report.status.iTow = -1;
}

void UBXComms::deinit() {
    if (m_is_init) {
        nrf_libuarte_async_uninit(BSP::UARTAsync_Inits[m_instance].uart);
        m_is_init = false;
    }
}

void UBXComms::cancel_expect() {
	m_expect.enable = false;
}

void UBXComms::expect(UBX::MessageClass resp_cls, uint8_t resp_msg_id) {
	m_expect.resp_cls = (uint8_t)resp_cls;
	m_expect.resp_msg_id = resp_msg_id;
	m_expect.enable = true;
}

void UBXComms::send_with_expect(uint8_t *buffer, unsigned int sz, UBX::MessageClass resp_cls, uint8_t resp_msg_id) {
	if (m_is_send_busy) {
		DEBUG_TRACE("UBXComms: send is busy...");
		while(m_is_send_busy);
	}
	// Setup expect filter
	HeaderAndPayloadCRC *msg = (HeaderAndPayloadCRC *)buffer;
	m_expect.req_cls = (uint8_t)msg->msgClass;
	m_expect.req_msg_id = msg->msgId;
	m_expect.resp_cls = (uint8_t)resp_cls;
	m_expect.resp_msg_id = resp_msg_id;
	m_expect.enable = true;
	send(buffer, sz);
}

void UBXComms::send(uint8_t *buffer, unsigned int sz, bool notify_sent, bool use_ext_buffer) {
	if (m_is_send_busy) {
		DEBUG_TRACE("UBXComms: send is busy...");
		while(m_is_send_busy);
	}

	if (m_debug_enable)
	    DEBUG_TRACE("UBXComms->GNSS: buffer=%s", Binascii::hexlify(std::string((char *)buffer, sz)).c_str());

	// Check for non-local buffer
	if (!use_ext_buffer && buffer != m_tx_buffer) {
		// Copy buffer locally since we are sending asynchronously
		std::memcpy(m_tx_buffer, buffer, sz);
		buffer = m_tx_buffer;
	}

	// Mark send as busy
	m_is_send_busy = true;
	m_notify_sent = notify_sent;
	ret_code_t ret;
	if ((ret = nrf_libuarte_async_tx(BSP::UARTAsync_Inits[m_instance].uart,
			buffer, sz) != NRF_SUCCESS)) {
		m_is_send_busy = false;  // Send failed so clear busy flag
		DEBUG_ERROR("UBXComms::send: failed to send ret=%08x", (unsigned int)ret);
	}
}

void UBXComms::set_baudrate(unsigned int baud) {
	nrf_uarte_baudrate_t baudrate;
	unsigned int timeout_us;

	switch (baud) {
	case 9600:
		baudrate = NRF_UARTE_BAUDRATE_9600;
		timeout_us = 1250;
		break;
    case 38400:
        baudrate = NRF_UARTE_BAUDRATE_38400;
        timeout_us = 800;
        break;
    case 921600:
        baudrate = NRF_UARTE_BAUDRATE_921600;
        timeout_us = 250;
        break;
	case 460800:
	default:
		baudrate = NRF_UARTE_BAUDRATE_460800;
		timeout_us = 250;
		break;
	}

	// Stop receiver to prevent any possible comms errors
	nrf_libuarte_async_stop_rx(BSP::UARTAsync_Inits[m_instance].uart);

    // Reset the RX buffer
    m_rx_buffer_offset = 0;

	// Adjust baud rate
	nrf_uarte_baudrate_set(BSP::UARTAsync_Inits[m_instance].uart->p_libuarte->uarte, baudrate);

	// Modify the async timeout based on the baudrate selected
	// This will also stop/start the receiver
	nrf_libuarte_async_set_timeout(BSP::UARTAsync_Inits[m_instance].uart, timeout_us);

	// Restart receiver
	nrf_libuarte_async_start_rx(BSP::UARTAsync_Inits[m_instance].uart);
}

void UBXComms::start_dbd_filter() {
	m_dbd_filter_en = true;
}

void UBXComms::stop_dbd_filter() {
	m_dbd_filter_en = false;
}

void UBXComms::handle_tx_done() {
	m_is_send_busy = false;
	if (m_notify_sent)
		notify<UBXCommsEventSendComplete>({});
}

void UBXComms::handle_rx_buffer(uint8_t * buffer, unsigned int length) {

    if (m_debug_enable) {
        notify(UBXCommsEventDebug(buffer, length));
    }

	// Run DBD filter on the buffer
	if (m_dbd_filter_en) {
		run_dbd_filter(buffer, length);
	} else {
	    // Make sure we can accept this DMA buffer from the UART driver
	    if (length < (sizeof(m_rx_buffer) - m_rx_buffer_offset)) {
	        HeaderAndPayloadCRC *msg;
	        uint8_t *curr_buffer = m_rx_buffer;
	        unsigned int curr_length;

	        // Append DMA buffer into our own RX buffer since we might have
	        // partial messages received previously
	        std::memcpy(&m_rx_buffer[m_rx_buffer_offset], buffer, length);
	        m_rx_buffer_offset += length;
	        curr_length = m_rx_buffer_offset;

            while ((msg = parse_message(&curr_buffer, &curr_length))) {
                run_expect_filter(msg);
                run_nav_filter(msg);
            }

            // Shift any surplus remaining data to the start of RX buffer
            std::memcpy(m_rx_buffer, curr_buffer, curr_length);
            m_rx_buffer_offset = curr_length;

	    } else {
	        // Flush the current input buffer
	        m_rx_buffer_offset = 0;
	        notify(UBXCommsEventError(0x400));
	    }
	}

	// Free the buffer back to the driver
	nrf_libuarte_async_rx_free(BSP::UARTAsync_Inits[m_instance].uart, buffer, length);
}

void UBXComms::filter_buffer(uint8_t * buffer, unsigned int length) {
	// Parse individual messages
	HeaderAndPayloadCRC *msg;
	while ((msg = parse_message(&buffer, &length))) {
		run_expect_filter(msg);
	}
}

void UBXComms::handle_error(unsigned int error_type) {
	nrf_libuarte_async_stop_rx(BSP::UARTAsync_Inits[m_instance].uart);
	notify(UBXCommsEventError(error_type));
}

HeaderAndPayloadCRC *UBXComms::parse_message(uint8_t **buffer, unsigned int *length) {

	while (*length >= (sizeof(Header) + 2)) {

		// Find next SYNC1
		while (**buffer != SYNC_CHAR1 && *length) {
			(*buffer)++;
			(*length)--;
		}

		// Check indicated buffer length is sufficiently large
		if (*length >= (sizeof(Header) + 2)) {
			for (;;) {
				// Setup message candidate
				HeaderAndPayloadCRC *msg = (HeaderAndPayloadCRC *)*buffer;

				// Check sync bytes are present at head of buffer
				if (msg->syncChars[1] != SYNC_CHAR2)
					break;

				// Check length field is valid
				if ((msg->msgLength + sizeof(Header) + 2) > *length) {
					//DEBUG_TRACE("Insufficient bytes for msg");
					return nullptr; // Insufficient bytes in the buffer
				}

				// Compute CRC field
				uint8_t ck_a, ck_b;
				compute_crc((const uint8_t *)&msg->msgClass, msg->msgLength + sizeof(Header) - 2, ck_a, ck_b);
				if (ck_a != msg->payload[msg->msgLength] || ck_b != msg->payload[msg->msgLength+1])
					break;

				// We have a good message
				*buffer += (msg->msgLength + sizeof(Header) + 2);
				*length -= (msg->msgLength + sizeof(Header) + 2);
				return msg;
			}
		} else {
			//DEBUG_TRACE("Insufficient bytes for header");
			return nullptr;
		}

		// No valid message found, try to find another SYNC1
		if (*length) {
			(*length)--;
			(*buffer)++;
		}
	}

	return nullptr;
}

void UBXComms::compute_crc(const uint8_t * const buffer, const unsigned int length, uint8_t &ck_a, uint8_t &ck_b) {
	ck_a = 0;
	ck_b = 0;
    for (unsigned int i = 0; i < length; i++)
    {
        ck_a = ck_a + buffer[i];
        ck_b = ck_b + ck_a;
    }
}

void UBXComms::run_expect_filter(const UBX::HeaderAndPayloadCRC * const msg) {
	if (!m_expect.enable) return;
	if (m_expect.resp_cls != (uint8_t)msg->msgClass ||
		m_expect.resp_msg_id != msg->msgId) return;

	// Expect filter matches
	if (msg->msgClass == MessageClass::MSG_CLASS_ACK) {
		// Additionally check the request message ID matches
		ACK::MSG_ACK *ack = (ACK::MSG_ACK *)msg->payload;
		if ((uint8_t)ack->clsID != m_expect.req_cls ||
			ack->msgID != m_expect.req_msg_id)
			return;
		m_expect.enable = false;
		notify(UBXCommsEventAckNack(msg->msgId == m_expect.resp_msg_id));
	} else if (msg->msgClass == MessageClass::MSG_CLASS_MGA && msg->msgId == MGA::ID_ACK) {
		m_expect.enable = false;
		MGA::MSG_ACK *ack = (MGA::MSG_ACK *)msg->payload;
		if (m_dbd_filter_en) {
			notify(UBXCommsEventMgaAck((ack->infoCode == 0), (unsigned int)ack->msgPayloadStart));
		} else {
			m_expect.enable = false;
			notify(UBXCommsEventAckNack(ack->infoCode == 0));
		}
	} else {
		m_expect.enable = false;
		notify(UBXCommsEventAckNack(true));
	}
}

void UBXComms::run_nav_filter(const UBX::HeaderAndPayloadCRC * const msg) {
	if (msg->msgClass != MessageClass::MSG_CLASS_NAV) return;

	// Update navigation report
	if (msg->msgId == NAV::ID_PVT) {
		std::memcpy(&m_nav_report.pvt, msg->payload, sizeof(m_nav_report.pvt));
		//DEBUG_TRACE("PVT: itow=%u valid=%u fix=%u", m_nav_report.pvt.iTow, (unsigned int)m_nav_report.pvt.valid,
		//		(unsigned int)m_nav_report.pvt.fixType);
		if (m_nav_report.pvt.iTow != m_nav_report_iTOW)
			m_nav_report_iTOW = m_nav_report.pvt.iTow;
	} else if (msg->msgId == NAV::ID_DOP) {
		std::memcpy(&m_nav_report.dop, msg->payload, sizeof(m_nav_report.dop));
		//DEBUG_TRACE("DOP: itow=%u", m_nav_report.dop.iTow);
		if (m_nav_report.dop.iTow != m_nav_report_iTOW)
			m_nav_report_iTOW = m_nav_report.dop.iTow;
	} else if (msg->msgId == NAV::ID_STATUS) {
		std::memcpy(&m_nav_report.status, msg->payload, sizeof(m_nav_report.status));
		//DEBUG_TRACE("STATUS: itow=%u", m_nav_report.status.iTow);
		if (m_nav_report.status.iTow != m_nav_report_iTOW)
			m_nav_report_iTOW = m_nav_report.status.iTow;
    } else if (msg->msgId == NAV::ID_SAT) {
        std::memcpy(&m_sat_report.sat, msg->payload,
                    std::min((int)msg->msgLength, (int)sizeof(UBX::NAV::SAT::MSG_SAT)));
        // Limit maximum number of SVs that can be reported to size of msg
		//DEBUG_TRACE("SAT: nsv=%u", m_sat_report.sat.numSvs);
        if (m_sat_report.sat.numSvs > 12)
        	m_sat_report.sat.numSvs = 12;
        notify(m_sat_report);
	} else {
		return;
	}

	// Check for matching iTOW and valid fix
	if (m_nav_report_iTOW == m_nav_report.status.iTow &&
		m_nav_report_iTOW == m_nav_report.pvt.iTow &&
		m_nav_report_iTOW == m_nav_report.dop.iTow) {
		m_nav_report_iTOW = -1; // Reset this report as now notified
		notify(m_nav_report);
	}
}

void UBXComms::run_dbd_filter(uint8_t *buffer, unsigned int length) {
	notify(UBXCommsEventMgaDBD(buffer, length));
}

bool UBXComms::is_expected_msg_count(uint8_t *buffer, unsigned int length, unsigned int expected,
		unsigned int& actual_count,
		MessageClass msg_cls, uint8_t msg_id) {

	HeaderAndPayloadCRC *msg;
	unsigned int count = 0;
	uint8_t *curr_buffer = buffer;
	unsigned int curr_length = length;

	// Count number of records in buffer
	while ((msg = parse_message(&curr_buffer, &curr_length))) {
		if (msg->msgClass == msg_cls && msg->msgId == msg_id)
			count++;
	}

	actual_count = count;

	return (count == expected);
}

void UBXComms::copy_mga_ano_to_buffer(File& file, uint8_t *dest_buffer, const unsigned int buffer_size, std::time_t now,
		unsigned int& num_bytes_copied, unsigned int& num_msg_copied,
		unsigned int& ano_start_pos) {
	uint8_t buffer[UBX::MAX_PACKET_LEN];
	lfs_soff_t offset = ano_start_pos;
	num_bytes_copied = 0;
	num_msg_copied = 0;
	std::time_t deltatime = (std::time_t)0xFFFFFFFF;

	DEBUG_TRACE("UBXComms::copy_mga_ano_to_buffer: parsing file size %u bytes from pos %u", (unsigned int)file.size(), ano_start_pos);

	// Try to seek to start position if non-zero
	if (offset) {
		file.seek(offset);
	}

	// Iterate each message of the file
	while (true) {

		// Read UBX header into buffer
		lfs_ssize_t sz;
		sz = file.read(buffer, sizeof(Header));

		//DEBUG_TRACE("UBXComms::copy_mga_ano_to_buffer: read msg @ offset=%u", offset);

		// Check header read was successful
		if (sz == (lfs_ssize_t)(sizeof(Header))) {
			HeaderAndPayloadCRC *msg = (HeaderAndPayloadCRC *)buffer;
			unsigned int msg_len = msg->msgLength + sizeof(Header) + 2;

			//DEBUG_TRACE("UBXComms::copy_mga_ano_to_buffer: payload_sz=%u msg_len=%u", (unsigned int)msg->msgLength, msg_len);

			if (msg_len <= UBX::MAX_PACKET_LEN) {
				// Read remaining payload and CRC
				sz = file.read(&buffer[sizeof(Header)], (msg->msgLength + 2));
				if (sz == (lfs_ssize_t)(msg->msgLength + 2)) {

					// Filter on MGA-ANO messages only
					if (msg->msgClass == MessageClass::MSG_CLASS_MGA && msg->msgId == MGA::ID_ANO) {
						MGA::MSG_ANO *ano = (MGA::MSG_ANO *)msg->payload;
						std::time_t ano_time = convert_epochtime(2000 + ano->year, ano->month, ano->day, 12, 0, 0);
						std::time_t timediff = std::abs(ano_time - now);
						if (timediff < deltatime) {
							//DEBUG_TRACE("UBXComms::copy_mga_ano_to_buffer: new delta=%08x < old=%08x", (unsigned int)timediff, (unsigned int)deltatime);
							deltatime = timediff;
							num_bytes_copied = 0;
							num_msg_copied = 0;
							ano_start_pos = (unsigned int)offset;
						}
						else if (timediff > deltatime) {
							//DEBUG_TRACE("UBXComms::copy_mga_ano_to_buffer: new delta=%08x > old=%08x", (unsigned int)timediff, (unsigned int)deltatime);
							DEBUG_TRACE("UBXComms::copy_mga_ano_to_buffer: copied %u msgs from pos %u", num_msg_copied, ano_start_pos);
							break;  // Time delta is worse, so do not proceed with these messages
						}

						// Copy message if it fits
						if ((num_bytes_copied + msg_len) < buffer_size) {
							//DEBUG_TRACE("UBXComms::copy_mga_ano_to_buffer: copy %u total %u", msg_len, num_bytes_copied);
							std::memcpy(&dest_buffer[num_bytes_copied], buffer, msg_len);
							num_bytes_copied += msg_len;
							num_msg_copied++;
						} else {
							DEBUG_WARN("UBXComms::copy_mga_ano_to_buffer: ANO buffer overflow");
							break;
						}
					}
					offset += msg_len;
				} else {
					DEBUG_WARN("UBXComms::copy_mga_ano_to_buffer: unexpected EOF");
					break;
				}
			} else {
				DEBUG_WARN("UBXComms::copy_mga_ano_to_buffer: unexpected msg size");
				break;
			}
		} else {
			DEBUG_TRACE("UBXComms::copy_mga_ano_to_buffer: EOF");
			break;
		}
	}

	// If deltatime > 24 hours then the database is stale and we should no longer process
	// this file
	if (deltatime >= (24*3600)) {
	    DEBUG_TRACE("UBXComms::copy_mga_ano_to_buffer: ANO file is stale");
	    ano_start_pos = file.size();
	    num_bytes_copied = 0;
	    num_msg_copied = 0;
	}
}
