#pragma once

#include <cstdint>

#include "bsp.hpp"

class NrfI2C {
private:
	static inline bool m_is_enabled[BSP::I2C_TOTAL_NUMBER] = { false };
public:
	static void init(void);
	static void uninit(void);
	static void disable(uint8_t bus);
	static void read(uint8_t bus, uint8_t address, uint8_t *buffer, unsigned int length);
	static void write(uint8_t bus, uint8_t address, const uint8_t *buffer, unsigned int length, bool no_stop);
	static uint8_t num_buses(void);
};
