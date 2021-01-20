#ifndef _NRF_SPIM_HPP__
#define _NRF_SPIM_HPP__

#include <stdint.h>

#include "spim.hpp"

class NrfSPIM final : public SPIM {
private:
	unsigned int m_instance;
	volatile bool m_xfer_done;

public:
	NrfSPIM(unsigned int instance);
	~NrfSPIM();
	int transfer(const uint8_t *tx_data, uint8_t *rx_data, uint16_t size) override;
	int transfer_continuous(const uint8_t *tx_data, uint8_t *rx_data, uint16_t size) override;
	int finish_transfer() override;
};

#endif // _NRF_SPIM_HPP__
