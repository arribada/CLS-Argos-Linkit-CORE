#include "nrf_i2c.hpp"
#include "nrfx_twim.h"
#include "error.hpp"
#include "debug.hpp"

void NrfI2C::init(void) {
	for (unsigned int i = 0; i < BSP::I2C_TOTAL_NUMBER; i++)
	{
		if (nrfx_twim_init(&BSP::I2C_Inits[i].twim, &BSP::I2C_Inits[i].twim_config, nullptr, nullptr) != NRFX_SUCCESS)
			throw ErrorCode::RESOURCE_NOT_AVAILABLE;
		nrfx_twim_enable(&BSP::I2C_Inits[i].twim);
		m_is_enabled[i] = true;
	}
}

void NrfI2C::uninit(void) {
	for (unsigned int i = 0; i < BSP::I2C_TOTAL_NUMBER; i++)
	{
		nrfx_twim_disable(&BSP::I2C_Inits[i].twim);
		nrfx_twim_uninit(&BSP::I2C_Inits[i].twim);
		m_is_enabled[i] = false;
	}
}

void NrfI2C::disable(uint8_t bus) {
	nrfx_twim_disable(&BSP::I2C_Inits[bus].twim);
	nrfx_twim_uninit(&BSP::I2C_Inits[bus].twim);
	m_is_enabled[bus] = false;
}

void NrfI2C::read(uint8_t bus, uint8_t address, uint8_t *buffer, unsigned int length) {
	if (!m_is_enabled[bus])
		throw ErrorCode::RESOURCE_NOT_AVAILABLE;
    nrfx_err_t error = nrfx_twim_rx(&BSP::I2C_Inits[bus].twim, address, buffer, length);
    if (NRFX_SUCCESS != error) {
    	DEBUG_ERROR("NrfI2C::read(%u,%02x,%u)=%08x", (unsigned int)bus, (unsigned int)address, (unsigned int)length, (unsigned int)error);
        throw ErrorCode::I2C_COMMS_ERROR;
    }
}

void NrfI2C::write(uint8_t bus, uint8_t address, const uint8_t *buffer, unsigned int length, bool no_stop) {
	if (!m_is_enabled[bus])
		throw ErrorCode::RESOURCE_NOT_AVAILABLE;
	nrfx_err_t error = nrfx_twim_tx(&BSP::I2C_Inits[bus].twim, address, buffer, length, no_stop);
    if (NRFX_SUCCESS != error) {
    	DEBUG_ERROR("NrfI2C::write(%u,%02x,%u)=%08x", (unsigned int)bus, (unsigned int)address, (unsigned int)length, (unsigned int)error);
        throw ErrorCode::I2C_COMMS_ERROR;
    }
}

uint8_t NrfI2C::num_buses(void) {
	return BSP::I2C::I2C_TOTAL_NUMBER;
}
