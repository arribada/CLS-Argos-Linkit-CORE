#include "ms58xx.hpp"

#include "bsp.hpp"
#include "nrf_i2c.hpp"
#include "error.hpp"
#include "pmu.hpp"
#include "debug.hpp"

// Converter functions for different device variants
static void read_ms5837_30bar(const int32_t D1, const int32_t D2, const uint16_t *C, double& temperature, double& pressure) {
	// working variables
	int32_t TEMP, dT;
	int64_t OFF, SENS, T2, OFF2, SENS2;

	/* Calculate first order coefficients */

	// Difference between actual and reference temperature
	// dT = D2 - C5 * 2^8
	dT = D2 - ((int32_t)C[5] << 8);

	// Actual temperature
	// TEMP = 2000 + dT * C6 / 2^23
	TEMP = (((int64_t)dT * C[6]) >> 23) + 2000;

	// Offset at actual temperature
	// OFF = C2 * 2^16 + ( C4 * dT ) / 2^7
	OFF = ((int64_t)C[2] << 16) + (((C[4] * (int64_t)dT)) >> 7);

	// Sensitivity at actual temperature
	// SENS = C1 * 2^15 + ( C3 * dT ) / 2^8
	SENS = ((int64_t)C[1] << 15) + (((C[3] * (int64_t)dT)) >> 8);

	/* Calculate second order coefficients */

	if (TEMP < 2000) // If temp is below 20.0C
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

	/* Calculate pressure */

	// P = (D1 * SENS / 2^21 - OFF) / 2^13
    pressure = (double)((((SENS * D1) >> 21 ) - OFF) >> 13) / 10000.0; // Convert to bar
    temperature = TEMP / 100.0;

    DEBUG_TRACE("MS58xx::read_ms5837_30bar: %f bar @ %f deg C", pressure, temperature);
}

static void read_ms5803_14bar(const int32_t D1, const int32_t D2, const uint16_t *C, double& temperature, double& pressure)
{
    // working variables
    int32_t TEMP, dT;
    int64_t OFF, SENS, T2, OFF2, SENS2;

    /* Calculate first order coefficients */
    // Difference between actual and reference temperature
    // dT = D2 - C5 * 2^8
    dT = D2 - ((int32_t)C[5] << 8);

    // Actual temperature
    // TEMP = 2000 + dT * C6 / 2^23
    TEMP = (((int64_t)dT * C[6]) >> 23) + 2000;

    // Offset at actual temperature
    // OFF = C2 * 2^16 + (C4 * dT ) / 2^7
    OFF = ((int64_t)C[2] << 16) + (((C[4] * (int64_t)dT)) >> 7);

    // Sensitivity at actual temperature
    // SENS = C1 * 2^15 + ( C3 * dT ) / 2^8
    SENS = ((int64_t)C[1] << 15) + (((C[3] * (int64_t)dT)) >> 8);

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

    DEBUG_TRACE("MS58xx::read_ms5803_14bar: %f bar @ %f deg C", pressure, temperature);
}

static void read_ms5837_02bar(const int32_t D1, const int32_t D2, const uint16_t *C, double& temperature, double& pressure) {
    // working variables
    int32_t TEMP, dT;
    int64_t OFF, SENS, T2, OFF2, SENS2;

    /* Calculate first order coefficients */

    // Difference between actual and reference temperature
    // dT = D2 - C5 * 2^8
    dT = D2 - ((int32_t)C[5] << 8);

    // Actual temperature
    // TEMP = 2000 + dT * C6 / 2^23
    TEMP = (((int64_t)dT * C[6]) >> 23) + 2000;

    // Offset at actual temperature
    // OFF = C2 * 2^17 + (C4 * dT ) / 2^6
    OFF = ((int64_t)C[2] << 17) + (((C[4] * (int64_t)dT)) >> 6);

    // Sensitivity at actual temperature
    // SENS = C1 * 2^16 + ( C3 * dT ) / 2^7
    SENS = ((int64_t)C[1] << 16) + (((C[3] * (int64_t)dT)) >> 7);

    /* Calculate second order coefficients */

    if (TEMP < 2000) // If temp is below 20.0C
    {
        // T2 = 11 * dT^2 / 2^35
        T2 = 3 * (((int64_t)dT * dT) >> 35);

        // OFF2 = 31 * (TEMP - 2000) ^ 2 / 2^3
        OFF2 = 3 * ((TEMP - 2000) * (TEMP - 2000)) >> 3;

        // SENS2 = 63 * (TEMP - 2000) ^ 2 / 2^5
        SENS2 = 63 * ((TEMP - 2000) * (TEMP - 2000)) >> 5;
    }
    else // If TEMP is above 20.0C
    {
    	T2 = 0;
    	OFF2 = 0;
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

    DEBUG_TRACE("MS58xx::read_ms5837_02bar: %f bar @ %f deg C", pressure, temperature);
}

MS58xxLL::MS58xxLL(unsigned int bus, unsigned char addr, std::string variant) {
	DEBUG_TRACE("MS58xxLL::MS58xxLL(i2cbus=%u, i2caddr=0x%02x, variant=%s)", bus, (unsigned int)addr, variant.c_str());
	m_bus = bus;
	m_addr = addr;
	m_resolution = MS58xxCommand::ADC_4096;

	// Unfortunately this family of devices has no consistent way of determining its
	// product code so we have to pass the variant into the constructor.  This
	// affects a number of internal driver operations
	if (variant == "MS5837_02BA") {
		m_convert = read_ms5837_02bar;
		m_crc_offset = 0;
		m_prom_size = 7;
		m_crc_mask = 0x0FFF;
		m_crc_shift = 12;
		C[7] = 0;
	} else if (variant == "MS5837_30BA") {
		m_convert = read_ms5837_30bar;
		m_crc_offset = 0;
		m_prom_size = 7;
		m_crc_mask = 0x0FFF;
		m_crc_shift = 12;
	} else if (variant == "MS5803_14BA") {
		m_convert = read_ms5803_14bar;
		m_prom_size = 8;
		m_crc_offset = 7;
		m_crc_mask = 0xFF00;
		m_crc_shift = 0;
	} else {
		DEBUG_ERROR("MS58xxLL::MS58xxLL: unsupported device variant");
		throw ErrorCode::NOT_IMPLEMENTED; // Unsupported option
	}
	read_coeffs();
	check_coeffs();
}

void MS58xxLL::read(double& temperature, double& pressure) {
	int32_t D1 = (int32_t)sample_adc(MS58xxCommand::ADC_D1);
	int32_t D2 = (int32_t)sample_adc(MS58xxCommand::ADC_D2);
	m_convert(D1, D2, C, temperature, pressure);
}

void MS58xxLL::send_command(uint8_t command)
{
	NrfI2C::write(m_bus, m_addr, (const uint8_t *)&command, sizeof(command), false);
}

void MS58xxLL::read_coeffs()
{
	DEBUG_TRACE("MS58xxLL::read_coeffs: PROM size = %u words", m_prom_size);

	// Reset the device first
	send_command(MS58xxCommand::RESET);
	PMU::delay_ms(10);

    for (uint32_t i = 0; i < m_prom_size; ++i)
    {
        uint8_t read_buffer[2];

        send_command((uint8_t)MS58xxCommand::PROM | (i * 2));
        NrfI2C::read(m_bus, m_addr, read_buffer, 2);
        C[i] = (uint16_t) (((uint16_t)read_buffer[0] << 8) + read_buffer[1]);
    }
}

uint32_t MS58xxLL::sample_adc(uint8_t measurement)
{
	uint8_t cmd = (uint8_t)MS58xxCommand::ADC_CONV | (uint8_t)MS58xxCommand::ADC_4096 | measurement;
    send_command(cmd);
    PMU::delay_ms(10);
    send_command((uint8_t)MS58xxCommand::ADC_READ);
    uint8_t read_buffer[3];
    NrfI2C::read(m_bus, m_addr, read_buffer, 3);
    return ((uint32_t)read_buffer[0] << 16) + ((uint32_t)read_buffer[1] << 8) + read_buffer[2];
}

void MS58xxLL::check_coeffs()
{
	DEBUG_TRACE("MS58xxLL::check_coeffs");

	uint32_t n_rem = 0; // crc reminder

    uint16_t crc_temp = C[m_crc_offset]; // Save PROM coeff containing the CRC field

    C[m_crc_offset] &= m_crc_mask; // Zero out the CRC field

    // Compute CRC over entire PROM
    for (uint32_t i = 0; i < 16; ++i)
    {
        // choose LSB or MSB
        if (i % 2 == 1)
            n_rem ^= (C[i >> 1]) & 0x00FF;
        else
            n_rem ^= (C[i >> 1] >> 8);

        for (uint32_t j = 8; j > 0; j--) // For each bit in a byte
        {
            if (n_rem & (0x8000))
                n_rem = (n_rem << 1) ^ 0x3000;
            else
                n_rem = (n_rem << 1);
        }
    }

    n_rem = (0x000F & (n_rem >> 12)); // final 4-bit reminder is CRC code

    C[m_crc_offset] = crc_temp; // restore the crc_read to its original place

    uint8_t actual_crc = n_rem ^ 0x00;

    if (actual_crc != ((uint8_t)(crc_temp >> m_crc_shift))) {
    	DEBUG_TRACE("MS58xxLL::check_coeffs: CRC failure");
        throw ErrorCode::I2C_CRC_FAILURE;
    }
}
