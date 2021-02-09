#ifndef _NRF_UART_M8_HPP_
#define _NRF_UART_M8_HPP_

#include <stdint.h>
#include <array>

#include "uart.hpp"
#include "nrfx_uarte.h"

// A special Nrf UART driver that retrieves full UBX strings
class NrfUARTM8 final : public UART {
public:
	NrfUARTM8(unsigned int instance);
	~NrfUARTM8();
	int send(const uint8_t * data, uint32_t size) override;
	int receive(uint8_t * data, uint32_t size) override;

private:
	unsigned int m_instance;
	uint8_t rx_byte;
	static constexpr size_t RX_BUF_LEN = 256;
	uint32_t bytes_in_rx_buffer;
	std::array<uint8_t, RX_BUF_LEN> rx_buffer;

	volatile enum
	{
		WAITING_SYNC_CHAR_1,
		WAITING_SYNC_CHAR_2,
		WAITING_CLASS,
		WAITING_ID,
		WAITING_LENGTH_LOWER,
		WAITING_LENGTH_UPPER,
		WAITING_PAYLOAD_AND_CHECKSUM,
		WAITING_BUFFER_FREE
	} m_state;

	struct __attribute__((__packed__)) Header
    {
        uint8_t  syncChars[2];
        uint8_t  msgClass;
        uint8_t  msgId;
        uint16_t msgLength; /* Excludes header and CRC bytes */
	};

	static void static_event_handler(nrfx_uarte_event_t const * p_event, void * p_context);
	void event_handler(nrfx_uarte_event_t const * p_event);

	void update_state(uint8_t new_byte);
};

#endif // _NRF_UART_M8_HPP_
