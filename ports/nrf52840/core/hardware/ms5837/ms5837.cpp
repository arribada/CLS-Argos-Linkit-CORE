#include "bsp.hpp"
#include "ms5837.hpp"
#include "nrfx_twim.h"
#include "error.hpp"
#include "pmu.hpp"

MS5837::MS5837() : UWDetector()
{
	// This will raise an exception if the sensor is not present which should be caught
	// by the main application
	read_coeffs();
	check_coeffs();
}

bool MS5837::detector_state()
{
	return (read() >= m_activation_threshold);
}

double MS5837::read()
{
	read_coeffs();
	check_coeffs();

	int32_t D1 = (int32_t)sample_adc(MS5837Command::ADC_D1);
	int32_t D2 = (int32_t)sample_adc(MS5837Command::ADC_D2);

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
    // OFF = C2 * 2^16 + ( C4 * dT ) / 2^7
    OFF = ((int64_t)m_coefficients[2] << 16) + (((m_coefficients[4] * (int64_t)dT)) >> 7);

    // Sensitivity at actual temperature
    // SENS = C1 * 2^15 + ( C3 * dT ) / 2^8
    SENS = ((int64_t)m_coefficients[1] << 15) + (((m_coefficients[3] * (int64_t)dT)) >> 8);

    /* Calculate second order coefficients */

    if (TEMP / 100 < 20) // If temp is below 20.0C
    {
        // T2 = 3 * dT^2 / 2^33
        T2 = 3 * (((int64_t)dT * dT) >> 33);

        // OFF2 = 3 * (TEMP - 2000) ^ 2 / 2^1
        OFF2 = 3 * ((TEMP - 2000) * (TEMP - 2000)) >> 1;

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
        // T2 = 2 * dT^2 / 2^37
        T2 = 2 * (((int64_t)dT * dT) >> 37);

        // OFF2 = 1 * (TEMP - 2000) ^ 2 / 2^4
        OFF2 = 1 * ((TEMP - 2000) * (TEMP - 2000)) >> 4;

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

    // P = (D1 * SENS / 2^21 - OFF) / 2^13 in 10ths of a millibar
    return (double)((((SENS * D1) >> 21 ) - OFF) >> 13) / 10000.0; // Convert to bar
}

void MS5837::send_command(uint8_t command)
{
    if (NRFX_SUCCESS != nrfx_twim_tx(&BSP::I2C_Inits[MS5837_DEVICE].twim, MS5837_ADDRESS, (const uint8_t *)&command, sizeof(command), false))
        throw ErrorCode::I2C_COMMS_ERROR;
}

void MS5837::read_coeffs()
{
    for (uint32_t i = 0; i < 7; ++i)
    {
        uint8_t read_buffer[2];

        send_command((uint8_t)MS5837Command::PROM | (i * 2));
        if (NRFX_SUCCESS != nrfx_twim_rx(&BSP::I2C_Inits[MS5837_DEVICE].twim, MS5837_ADDRESS, read_buffer, 2))
            throw ErrorCode::I2C_COMMS_ERROR;

        m_coefficients[i] = (uint16_t) (((uint16_t)read_buffer[0] << 8) + read_buffer[1]);
    }
}

uint32_t MS5837::sample_adc(uint8_t measurement)
{
    send_command((uint8_t)MS5837Command::ADC_CONV | (uint8_t)MS5837Command::ADC_8192 | measurement);
    PMU::delay_ms(20);
    uint8_t read_buffer[3];
    if (NRFX_SUCCESS != nrfx_twim_rx(&BSP::I2C_Inits[MS5837_DEVICE].twim, MS5837_ADDRESS, read_buffer, 3))
        throw ErrorCode::I2C_COMMS_ERROR;
    return ((uint32_t)read_buffer[0] << 16) + ((uint32_t)read_buffer[1] << 8) + read_buffer[2];
}

void MS5837::check_coeffs()
{
    uint32_t n_rem = 0; // crc reminder

    uint16_t crc_temp = m_coefficients[0]; // Save the end of the configuration data

    m_coefficients[0] &= 0x0FFF; // CRC byte is replaced by 0
    m_coefficients[7] = 0; // Subsidiary value, set to 0

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

    m_coefficients[0] = crc_temp; // restore the crc_read to its original place

    uint8_t actual_crc = n_rem ^ 0x00;

    if (actual_crc != (crc_temp >> 12))
        throw ErrorCode::I2C_CRC_FAILURE;
}
