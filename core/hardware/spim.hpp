#ifndef _SPIM_HPP__
#define _SPIM_HPP__

#include <stdint.h>

class SPIM {
public:
	virtual ~SPIM() {}
	virtual int transfer(const uint8_t *tx_data, uint8_t *rx_data, uint16_t size) = 0;
	virtual int transfer_continuous(const uint8_t *tx_data, uint8_t *rx_data, uint16_t size) = 0;
	virtual int finish_transfer() = 0;
};

#endif // _SPIM_HPP__
