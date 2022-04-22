#include "ms5803.hpp"

#include "bsp.hpp"
#include "nrfx_twim.h"
#include "error.hpp"
#include "pmu.hpp"
#include "debug.hpp"


MS5803LL::MS5803LL(unsigned int bus, unsigned char addr) {
	DEBUG_TRACE("MS5803LL::MS5803LL(%u, 0x%02x)", bus, (unsigned int)addr);
	m_bus = bus;
	m_addr = addr;
	read_coeffs();
	check_coeffs();
}

void MS5803LL::read(double& temperature, double& pressure)
{
	int32_t D1 = (int32_t)sample_adc(MS5803Command::ADC_D1);
	int32_t D2 = (int32_t)sample_adc(MS5803Command::ADC_D2);

    // working variables
    int32_t TEMP, dT;
    int64_t OFF, SENS, T2, OFF2, SENS2;

    /* Calculate first order coefficients */

    // Difference between actual and reference temperature
    // dT = D2 - C5 * 2^8
    dT = D2 - ((int32_t)m_coefficients[5] << 8);

    // Actual temperature
    // TEMP = 2000 + dT * C6 / 2^23
    TEMP = (((int64_t)dT * m_coefficients[6]) >> 23) + 2000;

    // Offset at actual temperature
    // OFF = C2 * 2^16 + (C4 * dT ) / 2^7
    OFF = ((int64_t)m_coefficients[2] << 16) + (((m_coefficients[4] * (int64_t)dT)) >> 7);

    // Sensitivity at actual temperature
    // SENS = C1 * 2^15 + ( C3 * dT ) / 2^8
    SENS = ((int64_t)m_coefficients[1] << 15) + (((m_coefficients[3] * (int64_t)dT)) >> 8);

    /* Calculate second order coefficients */

    if (TEMP < 2000) // If temp is below 20.0C
    {
        // T2 = 3 * dT^2 / 2^33
        T2 = 3 * (((int64_t)dT * dT) >> 33);

        // OFF2 = 3 * (TEMP - 2000) ^ 2 / 2
        OFF2 = 3 * ((TEMP - 2000) * (TEMP - 2000)) / 2;

        // SENS2 = 5 * (TEMP - 2000) ^ 2 / 2^3
        SENS2 = 5 * ((TEMP - 2000) * (TEMP - 2000)) >> 3;

        if (TEMP < -1500) // If temp is below -15.0C
        {
            // OFF2 = OFF2 + 7 * (TEMP + 1500)^2
            OFF2 = OFF2 + 7 * ((TEMP + 1500) * (TEMP + 1500));

            // SENS2 = SENS2 + 4 * (TEMP + 1500)^2
            SENS2 = SENS2 + 4 * ((TEMP + 1500) * (TEMP + 1500));
        }
    }
    else // If TEMP is above 20.0C
    {
        // T2 = 7 * dT^2 / 2^37
        T2 = 7 * ((uint64_t)dT * dT) / (1ULL<<37);

        // OFF2 = 1 * (TEMP - 2000)^2 / 2^4
        OFF2 = ((TEMP - 2000) * (TEMP - 2000)) / 16;

        // SENS2 = 0
        SENS2 = 0;
    }

    /* Merge first and second order coefficients */

    // TEMP = TEMP - T2
    TEMP = TEMP - T2;

    // OFF = OFF - OFF2
    OFF = OFF - OFF2;

    // SENS = SENS - SENS2
    SENS = SENS - SENS2;

    /* Calculate pressure */

    // P = (D1 * SENS / 2^21 - OFF) / 2^15
    pressure = (double)((((SENS * D1) >> 21 ) - OFF) >> 15) / 10000.0; // Convert to bar
    temperature = TEMP / 100.0;

    DEBUG_TRACE("MS5803::read: %f bar @ %f deg C", pressure, temperature);
}


void MS5803LL::send_command(uint8_t command)
{
    if (NRFX_SUCCESS != nrfx_twim_tx(&BSP::I2C_Inits[m_bus].twim, m_addr, (const uint8_t *)&command, sizeof(command), false))
        throw ErrorCode::I2C_COMMS_ERROR;
}

void MS5803LL::read_coeffs()
{
	DEBUG_TRACE("MS5803LL::read_coeffs");

	// Reset the device first
	send_command(MS5803Command::RESET);
	PMU::delay_ms(10);

    for (uint32_t i = 0; i < 8; ++i)
    {
        uint8_t read_buffer[2];

        send_command((uint8_t)MS5803Command::PROM | (i * 2));
        if (NRFX_SUCCESS != nrfx_twim_rx(&BSP::I2C_Inits[m_bus].twim, m_addr, read_buffer, 2))
            throw ErrorCode::I2C_COMMS_ERROR;

        m_coefficients[i] = (uint16_t) (((uint16_t)read_buffer[0] << 8) + read_buffer[1]);
    }
}

uint32_t MS5803LL::sample_adc(uint8_t measurement)
{
	uint8_t cmd = (uint8_t)MS5803Command::ADC_CONV | (uint8_t)MS5803Command::ADC_4096 | measurement;
    send_command(cmd);
    PMU::delay_ms(10);
    send_command((uint8_t)MS5803Command::ADC_READ);
    uint8_t read_buffer[3];
    if (NRFX_SUCCESS != nrfx_twim_rx(&BSP::I2C_Inits[m_bus].twim, m_addr, read_buffer, 3))
        throw ErrorCode::I2C_COMMS_ERROR;
    return ((uint32_t)read_buffer[0] << 16) + ((uint32_t)read_buffer[1] << 8) + read_buffer[2];
}

void MS5803LL::check_coeffs()
{
	DEBUG_TRACE("MS5803LL::check_coeffs");

	uint32_t n_rem = 0; // crc reminder

    uint16_t crc_temp = m_coefficients[7]; // Save the end of the configuration data

    m_coefficients[7] &= 0xFF00; // CRC byte is replaced by 0

    for (uint32_t i = 0; i < 16; ++i)
    {
        // choose LSB or MSB
        if (i % 2 == 1)
            n_rem ^= (m_coefficients[i >> 1]) & 0x00FF;
        else
            n_rem ^= (m_coefficients[i >> 1] >> 8);

        for (uint32_t j = 8; j > 0; j--) // For each bit in a byte
        {
            if (n_rem & (0x8000))
                n_rem = (n_rem << 1) ^ 0x3000;
            else
                n_rem = (n_rem << 1);
        }
    }

    n_rem = (0x000F & (n_rem >> 12)); // final 4-bit reminder is CRC code

    m_coefficients[7] = crc_temp; // restore the crc_read to its original place

    uint8_t actual_crc = n_rem ^ 0x00;

    if (actual_crc != (uint8_t)crc_temp) {
    	DEBUG_TRACE("MS5803LL::check_coeffs: CRC failure");
        throw ErrorCode::I2C_CRC_FAILURE;
    }
}
