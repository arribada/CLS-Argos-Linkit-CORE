#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>
#include "ubx.hpp"
#include "events.hpp"
#include "error.hpp"
#include "debug.hpp"
#include "filesystem.hpp"
#include "timeutils.hpp"

class AsyncLowLevel;

enum UBXCommsMgaDbdStatus {
	SUCCESS,
	INCORRECT_NUM_MESSAGES
};
struct UBXCommsEventSatReport {
	UBX::NAV::SAT::MSG_SAT       sat;
};
struct UBXCommsEventNavReport {
    UBX::NAV::PVT::MSG_PVT       pvt;
    UBX::NAV::STATUS::MSG_STATUS status;
    UBX::NAV::DOP::MSG_DOP       dop;
};
struct UBXCommsEventSent {};
struct UBXCommsEventError {
    unsigned int error_type;
    UBXCommsEventError(unsigned int a) : error_type(a) {}
};
struct UBXCommsEventDebug {
	uint8_t *buffer;
	unsigned int length;
	UBXCommsEventDebug(uint8_t *a, unsigned int b) : buffer(a), length(b) {}
};
struct UBXCommsEventRespTimeout {};
struct UBXCommsEventAckNack {
	bool ack;
	UBXCommsEventAckNack(bool a) : ack(a) {}
};
struct UBXCommsEventMgaAck {
	bool ack;
	unsigned int num_dbd_messages;
	UBXCommsEventMgaAck(bool a, unsigned int b) : ack(a), num_dbd_messages(b) {}
};
struct UBXCommsEventMgaDBD {
	uint8_t * const database;
	unsigned int    length;
	UBXCommsEventMgaDBD(uint8_t * const a, unsigned int b) : database(a), length(b) {}
};

struct UBXCommsEventSendComplete {};

class UBXCommsEventListener {
public:
	virtual ~UBXCommsEventListener() {}
	virtual void react(const UBXCommsEventSendComplete&) {}
	virtual void react(const UBXCommsEventAckNack&) {}
	virtual void react(const UBXCommsEventMgaAck&) {}
	virtual void react(const UBXCommsEventNavReport&) {}
    virtual void react(const UBXCommsEventSatReport&) {}
	virtual void react(const UBXCommsEventMgaDBD&) {}
	virtual void react(const UBXCommsEventDebug&) {}
	virtual void react(const UBXCommsEventError&) {}
};

class UBXComms : public EventEmitter<UBXCommsEventListener> {
public:
	UBXComms(unsigned int libuarte_async_instance = 0);
	template <typename T>
	void send_packet_with_expect(UBX::MessageClass cls, uint8_t id, T content,
			UBX::MessageClass resp_cls = UBX::MessageClass::MSG_CLASS_ACK, uint8_t resp_msg_id = UBX::ACK::ID_ACK) {
		unsigned int content_size;
		if constexpr (std::is_same<T, UBX::Empty>::value) {
			content_size = 0;
		} else {
			content_size = sizeof(content);
		};

		if (m_is_send_busy) {
			DEBUG_TRACE("UBXComms: send is busy...");
			while(m_is_send_busy);
		}

		UBX::HeaderAndPayloadCRC *msg = (UBX::HeaderAndPayloadCRC *)m_tx_buffer;
		msg->syncChars[0] = UBX::SYNC_CHAR1;
		msg->syncChars[1] = UBX::SYNC_CHAR2;
		msg->msgClass = cls;
		msg->msgId = id;
		msg->msgLength = content_size;
		std::memcpy(msg->payload, &content, content_size);
		compute_crc((const uint8_t * const)&msg->msgClass, content_size + sizeof(UBX::Header) - 2, msg->payload[msg->msgLength], msg->payload[msg->msgLength+1]);
		send_with_expect(m_tx_buffer, content_size + sizeof(UBX::Header) + 2, resp_cls, resp_msg_id);
	}

	void init();
	void deinit();

	template <typename T>
	void send_packet(UBX::MessageClass cls, uint8_t id, T content) {

		if (m_is_send_busy) {
			DEBUG_TRACE("UBXComms: send is busy...");
			while(m_is_send_busy);
		}

		unsigned int content_size;
		if constexpr (std::is_same<T, UBX::Empty>::value) {
			content_size = 0;
		} else {
			content_size = sizeof(content);
		};
		UBX::HeaderAndPayloadCRC *msg = (UBX::HeaderAndPayloadCRC *)m_tx_buffer;
		msg->syncChars[0] = UBX::SYNC_CHAR1;
		msg->syncChars[1] = UBX::SYNC_CHAR2;
		msg->msgClass = cls;
		msg->msgId = id;
		msg->msgLength = content_size;
		std::memcpy(msg->payload, &content, content_size);
		compute_crc((const uint8_t * const)&msg->msgClass, content_size + sizeof(UBX::Header) - 2, msg->payload[msg->msgLength], msg->payload[msg->msgLength+1]);
		send(m_tx_buffer, content_size + sizeof(UBX::Header) + 2);
	}

	void filter_buffer(uint8_t *buffer, unsigned int length);
	void cancel_expect();
	void expect(UBX::MessageClass resp_cls, uint8_t resp_msg_id);
	void send_with_expect(uint8_t *buffer, unsigned int sz, UBX::MessageClass resp_cls = UBX::MessageClass::MSG_CLASS_ACK, uint8_t resp_msg_id = UBX::ACK::ID_ACK);
	void send(uint8_t *buffer, unsigned int sz, bool notify_sent = false, bool use_ext_buffer = false);
	void wait_send() { while (m_is_send_busy); }
	void set_baudrate(unsigned int baudrate);
	void start_dbd_filter();
	void stop_dbd_filter();
	bool is_expected_msg_count(uint8_t *buffer, unsigned int length, unsigned int expected,
			unsigned int& actual_count,
			UBX::MessageClass msg_cls, uint8_t msg_id);
	void copy_mga_ano_to_buffer(File& file, uint8_t *dest_buffer, const unsigned int buffer_size, std::time_t now,
			unsigned int& num_bytes_copied, unsigned int& num_msg_copied,
			unsigned int& ano_start_pos);
	void set_debug_enable(bool e) {
	    m_debug_enable = e;
	}

private:
	UBXCommsEventNavReport m_nav_report;
    UBXCommsEventSatReport m_sat_report;
	unsigned int m_instance;
	volatile bool m_is_send_busy;
	bool         m_notify_sent;
	unsigned int m_nav_report_iTOW;
	bool m_dbd_filter_en;
	bool m_debug_enable;
	struct Expect {
		bool    enable;
		uint8_t req_cls;
		uint8_t req_msg_id;
		uint8_t resp_cls;
		uint8_t resp_msg_id;
	} m_expect;
	uint8_t m_tx_buffer[256];
    uint8_t m_rx_buffer[512];
    unsigned int m_rx_buffer_offset;
	bool m_is_init;

	void handle_error(unsigned int);
	void handle_tx_done();
	void handle_rx_buffer(uint8_t * buffer, unsigned int length);
	UBX::HeaderAndPayloadCRC *parse_message(uint8_t ** buffer, unsigned int *length);
	void compute_crc(const uint8_t * const buffer, const unsigned int length, uint8_t &ck_a, uint8_t &ck_b);
	void run_expect_filter(const UBX::HeaderAndPayloadCRC * const msg);
	void run_nav_filter(const UBX::HeaderAndPayloadCRC * const msg);
	void run_dbd_filter(uint8_t *buffer, unsigned int length);

	friend AsyncLowLevel;
};
