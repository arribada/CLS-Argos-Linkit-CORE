#ifndef _NRF_UART_M8_HPP_
#define _NRF_UART_M8_HPP_

#include <stdint.h>
#include <functional>
#include <array>

#include "uart.hpp"
#include "nrfx_uarte.h"
#include "ubx.hpp"

// A special Nrf UART driver that retrieves full UBX strings
class NrfUARTM8 final : public UART {
public:
	NrfUARTM8(unsigned int instance, std::function<void(uint8_t *data, size_t len)> on_receive);
	~NrfUARTM8();
	int send(const uint8_t * data, uint32_t size) override;
	int receive(uint8_t * data, uint32_t size) override { return -1; }; // Reception is done through the on_receive callback
	void change_baudrate(uint32_t baudrate);

private:
	unsigned int m_instance;
	std::function<void(uint8_t *data, size_t len)> m_on_receive;
	uint8_t rx_byte;
	std::array<uint8_t, UBX::MAX_PACKET_LEN> rx_buffer;
	uint32_t m_bytes_to_read = 0;
	uint32_t m_bytes_read = 0;

	volatile enum
	{
		WAITING_SYNC_CHAR_1,
		WAITING_SYNC_CHAR_2,
		WAITING_CLASS,
		WAITING_ID,
		WAITING_LENGTH_LOWER,
		WAITING_LENGTH_UPPER,
		WAITING_PAYLOAD_AND_CHECKSUM
	} m_state;

	static void static_event_handler(nrfx_uarte_event_t const * p_event, void * p_context);
	void event_handler(nrfx_uarte_event_t const * p_event);

	void update_state(uint8_t new_byte);
};

#endif // _NRF_UART_M8_HPP_
