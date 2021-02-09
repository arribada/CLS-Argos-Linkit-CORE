#ifndef _UART_HPP__
#define _UART_HPP__

#include <stdint.h>

class UART {
public:
	virtual ~UART() {};
	virtual int send(const uint8_t * data, uint32_t size) = 0;
	virtual int receive(uint8_t * data, uint32_t size) = 0;
};

#endif // _UART_HPP__
