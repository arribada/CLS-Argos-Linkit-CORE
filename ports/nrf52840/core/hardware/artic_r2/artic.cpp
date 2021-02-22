#include <stdint.h>
#include <cstring>
#include <cmath>

#include "bsp.hpp"
#include "artic_firmware.hpp"
#include "artic_registers.hpp"
#include "error.hpp"
#include "debug.hpp"
#include "artic_firmware.hpp"
#include "artic.hpp"
#include "gpio.hpp"

#include "nrf_delay.h"
#include "nrf_gpio.h"

#define SAT_ARTIC_DELAY_TICK_INTERRUPT_MS 10
#define INVALID_MEM_SELECTION (0xFF)

#ifndef DEFAULT_TCXO_WARMUP_TIME_SECONDS
#define DEFAULT_TCXO_WARMUP_TIME_SECONDS 3
#endif

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define TX_FREQUENCY_ARGOS_2_3_BAND_START		401.62

static constexpr const char *const status_string[] =
{
    "IDLE",                                   // The firmware is idle and ready to accept commands.
    "RX_IN_PROGRESS",                         // The firmware is receiving.
    "TX_IN_PROGRESS",                         // The firmware is transmitting.
    "BUSY",                                   // The firmware is changing state.
    //      Interrupt 1 flags
    "RX_VALID_MESSAGE",                       // A message has been received.
    "RX_SATELLITE_DETECTED",                  // A satellite has been detected.
    "TX_FINISHED",                            // The transmission was completed.
    "MCU_COMMAND_ACCEPTED",                   // The configuration command has been accepted.
    "CRC_CALCULATED",                         // CRC calculation has finished.
    "IDLE_STATE",                             // Firmware returned to the idle state.
    "RX_CALIBRATION_FINISHED",                // RX offset calibration has completed.
    "RESERVED_11",
    "RESERVED_12",
    //      Interrupt 2 flags
    "RX_TIMEOUT",                             // The specified reception time has been exceeded.
    "SATELLITE_TIMEOUT",                      // No satellite was detected within the specified time.
    "RX_BUFFER_OVERFLOW",                     // A received message is lost. No buffer space left.
    "TX_INVALID_MESSAGE",                     // Incorrect TX payload length specified.
    "MCU_COMMAND_REJECTED",                   // Incorrect command send or Firmware is not in idle.
    "MCU_COMMAND_OVERFLOW",                   // Previous command was not yet processed.
    "RESERVED_19",
    "RESERVER_20",
    // Others
    "INTERNAL_ERROR",                         // An internal error has occurred.
    "dsp2mcu_int1",                           // Interrupt 1 pin status
    "dsp2mcu_int2"                            // Interrupt 2 pin status
};
static_assert(ARRAY_SIZE(status_string) == TOTAL_NUMBER_STATUS_FLAG);

inline uint8_t ArticTransceiver::convert_mem_sel(mem_id_t mode)
{
    switch (mode)
    {
        case PMEM:
            return 0;
            break;
        case XMEM:
            return 1;
            break;
        case YMEM:
            return 2;
            break;
        case IOMEM:
            return 3;
            break;
        case INVALID_MEM:
        default:
            return INVALID_MEM_SELECTION;
            break;
    }
}

inline ArticTransceiver::mem_id_t ArticTransceiver::convert_mode(uint8_t mem_sel)
{
    switch (mem_sel)
    {
        case 0:
            return PMEM;
            break;
        case 1:
            return XMEM;
            break;
        case 2:
            return YMEM;
            break;
        case 3:
            return IOMEM;
            break;
        default:
            return INVALID_MEM;
            break;
    }
}

static void reverse_memcpy(uint8_t *dst, const uint8_t *src, size_t size)
{
    for (uint32_t i = 0; i < size; ++i)
        dst[i] = src[size - 1 - i];
}

void ArticTransceiver::send_artic_command(artic_cmd_t cmd, uint32_t *response)
{
    uint8_t buffer_rx[4] = {0};
    uint8_t buffer_tx[4] = {0};

    switch (cmd)
    {
        case DSP_STATUS:
            buffer_tx[0] = ARTIC_READ_ADDRESS(ADDRESS_DSP);
            break;

        case DSP_CONFIG:
            buffer_tx[0] = ARTIC_WRITE_ADDRESS(ADDRESS_DSP);
            break;

        default:
            break;
    }

    int ret = m_nrf_spim->transfer(buffer_tx, buffer_rx, 4);
    if (ret) {
    	throw ErrorCode::SPI_COMMS_ERROR;
    }
    if (response)
        *response = (buffer_rx[1] << 16) | (buffer_rx[2] << 8) | buffer_rx[3];
}


void ArticTransceiver::check_crc(firmware_header_t *firmware_header)
{
    uint32_t crc;
    uint8_t crc_buffer[NUM_FIRMWARE_FILES_ARTIC * SIZE_SPI_REG_XMEM_YMEM_IOMEM]; // 3 bytes CRC for each firmware file

    spi_read(XMEM, CRC_ADDRESS, crc_buffer, NUM_FIRMWARE_FILES_ARTIC * SIZE_SPI_REG_XMEM_YMEM_IOMEM ); // 3 bytes CRC for each firmware local_file_id

    // Check CRC Value PMEM
    crc = 0;
    reverse_memcpy((uint8_t *)&crc, &crc_buffer[0], SIZE_SPI_REG_XMEM_YMEM_IOMEM);
    if (firmware_header->PMEM_CRC != crc)
    {
        DEBUG_ERROR("ArticTransceiver::check_crc: PMEM CRC 0x%08lX DOESN'T MATCH EXPECTED 0x%08lX", crc, firmware_header->PMEM_CRC);
        throw ErrorCode::ARTIC_CRC_FAILURE;
    }

    // Check CRC Value XMEM
    crc = 0;
    reverse_memcpy((uint8_t *)&crc, &crc_buffer[3], SIZE_SPI_REG_XMEM_YMEM_IOMEM);
    if (firmware_header->XMEM_CRC != crc)
    {
        DEBUG_ERROR("ArticTransceiver::check_crc: XMEM CRC 0x%08lX DOESN'T MATCH EXPECTED 0x%08lX", crc, firmware_header->XMEM_CRC);
        throw ErrorCode::ARTIC_CRC_FAILURE;
    }

    // Check CRC Value YMEM
    crc = 0;
    reverse_memcpy((uint8_t *)&crc, &crc_buffer[6], SIZE_SPI_REG_XMEM_YMEM_IOMEM);
    if (firmware_header->YMEM_CRC != crc)
    {
        DEBUG_ERROR("ArticTransceiver::check_crc: YMEM CRC 0x%08lX DOESN'T MATCH EXPECTED 0x%08lX", crc, firmware_header->YMEM_CRC);
        throw ErrorCode::ARTIC_CRC_FAILURE;
    }

    DEBUG_TRACE("ArticTransceiver::check_crc: CRC values all match");
}

void ArticTransceiver::configure_burst(mem_id_t mode, bool read, uint32_t start_address)
{
    uint8_t buffer_rx[4] = {0};
    uint8_t buffer_tx[4] = {0};
    uint32_t burst_reg = 0;
    uint8_t mem_sel;
    int ret;

    mem_sel = convert_mem_sel(mode);

    // Config burst register
    burst_reg |= BURST_MODE_SHIFT_BITMASK;
    burst_reg |= ((mem_sel << MEM_SEL_SHIFT) & MEM_SEL_BITMASK);
    if (read)
        burst_reg |= BURST_R_NW_MODE_BITMASK;

    burst_reg |= (start_address & BURST_START_ADDRESS_BITMASK);

    buffer_tx[0] = ARTIC_WRITE_ADDRESS(BURST_ADDRESS);
    buffer_tx[1] = burst_reg >> 16;
    buffer_tx[2] = burst_reg >> 8;
    buffer_tx[3] = burst_reg;

    ret = m_nrf_spim->transfer(buffer_tx, buffer_rx, 4);
    if (ret) {
        throw ErrorCode::SPI_COMMS_ERROR;
    }

    nrf_delay_ms(SAT_ARTIC_DELAY_SET_BURST_MS);
}

void ArticTransceiver::burst_access(mem_id_t mode, uint32_t start_address, const uint8_t *tx_data, uint8_t *rx_data, size_t size, bool read )
{
    uint8_t length_transfer;

    // Set burst register to configure a burst transfer
    configure_burst(mode, read, start_address);

    if (mode == PMEM) // PMEM is a 4 byte register
        length_transfer = 4;
    else // The rest are 3
        length_transfer = 3;

    send_burst(tx_data, rx_data, size, length_transfer, read);

    // Deactivate SSN pin
    m_nrf_spim->finish_transfer();

    nrf_delay_ms(SAT_ARTIC_DELAY_FINISH_BURST_MS);
}

void ArticTransceiver::send_burst(const uint8_t *tx_data, uint8_t *rx_data, size_t size, uint8_t length_transfer, bool read)
{
    uint16_t num_transfer = size / length_transfer;

    // Write in chunk of 60 bytes and wait 5 ms
    uint32_t delay_count = 0;
    uint8_t buffer[SIZE_SPI_REG_PMEM];
    int ret;

    // buffer for saving memory when not needed
    std::memset(buffer, 0, SIZE_SPI_REG_PMEM);

    if (read)
    {
        for (uint32_t i = 0; i < num_transfer; ++i)
        {
            ret = m_nrf_spim->transfer_continuous(buffer, &rx_data[i * length_transfer], length_transfer);
            if (ret) {
                throw ErrorCode::SPI_COMMS_ERROR;
            }

            delay_count += length_transfer;
            nrf_delay_us(SAT_ARTIC_DELAY_TRANSFER_US);
            if (delay_count > NUM_BYTES_BEFORE_WAIT)
            {
                delay_count = 0;
                nrf_delay_ms(SAT_ARTIC_DELAY_BURST_MS);
            }

        }
    }
    else
    {
        for (uint32_t i = 0; i < num_transfer; ++i)
        {
            ret = m_nrf_spim->transfer_continuous(&tx_data[i * length_transfer], buffer, length_transfer);
            if (ret) {
                throw ErrorCode::SPI_COMMS_ERROR;
            }
            delay_count += length_transfer;
            nrf_delay_us(SAT_ARTIC_DELAY_TRANSFER_US);
            if (delay_count > NUM_BYTES_BEFORE_WAIT)
            {
                delay_count = 0;
                nrf_delay_ms(SAT_ARTIC_DELAY_BURST_MS);
            }
        }
    }
}

void ArticTransceiver::spi_read(mem_id_t mode, uint32_t start_address, uint8_t *buffer_read, size_t size)
{
    std::memset(buffer_read, 0, size);
    burst_access(mode, start_address, NULL, buffer_read, size, true);
}

void ArticTransceiver::print_status(void)
{
    uint32_t status;
    get_status_register(&status);

    for (uint32_t i = 0; i < TOTAL_NUMBER_STATUS_FLAG; ++i)
    {
        if (status & (1 << i)) {
            DEBUG_TRACE("ArticTransceiver::print_status: %s", (char *)status_string[i]);
        }
    }
}

void ArticTransceiver::clear_interrupt(uint8_t interrupt_num)
{
    switch (interrupt_num)
    {
        case 1:
            send_command(ARTIC_CMD_CLEAR_INT1);
            break;
        case 2:
            send_command(ARTIC_CMD_CLEAR_INT2);
            break;

        default:
        	break;
    }
}

void ArticTransceiver::send_command(uint8_t command)
{
    uint8_t buffer_read;
    int ret;

    ret = m_nrf_spim->transfer(&command, &buffer_read, sizeof(command));
    if (ret) {
        throw ErrorCode::SPI_COMMS_ERROR;
    }
}

void ArticTransceiver::wait_interrupt(uint32_t timeout_ms, uint8_t interrupt_num)
{
    BSP::GPIO gpio_port;
    bool int_status;

    uint32_t iterations = timeout_ms / SAT_ARTIC_DELAY_TICK_INTERRUPT_MS;

    if (interrupt_num == INTERRUPT_1)
        gpio_port = BSP::GPIO::GPIO_INT1_SAT;
    else
        gpio_port = BSP::GPIO::GPIO_INT2_SAT;

    do
    {
        nrf_delay_ms(SAT_ARTIC_DELAY_TICK_INTERRUPT_MS);
        int_status = GPIOPins::value(gpio_port);
    }
    while (iterations-- && !int_status);

    if (int_status == false)
    {
        DEBUG_TRACE("ArticTransceiver::wait_interrupt: Waiting for interrupt_%u timed out", interrupt_num);
        throw ErrorCode::ARTIC_IRQ_TIMEOUT;
    }
}

void ArticTransceiver::send_command_check_clean(uint8_t command, uint8_t interrupt_number, uint8_t status_flag_number, bool value, uint32_t interrupt_timeout_ms)
{
    uint32_t status = 0;

    clear_interrupt(interrupt_number);
    send_command(command);
    wait_interrupt(interrupt_timeout_ms, interrupt_number);
    get_status_register(&status);

    if ((status & (1 << status_flag_number)) == !value) {
    	throw ErrorCode::ARTIC_INCORRECT_STATUS;
    }

    clear_interrupt(interrupt_number);
}

void ArticTransceiver::hardware_init()
{
	GPIOPins::clear(BSP::GPIO::GPIO_SAT_RESET);
	GPIOPins::clear(BSP::GPIO::GPIO_SAT_EN);
}

void ArticTransceiver::send_fw_files(void)
{
    uint8_t buffer[MAXIMUM_READ_FIRMWARE_OPERATION];
    mem_id_t order_mode[NUM_FIRMWARE_FILES_ARTIC] = {XMEM, YMEM, PMEM};
    firmware_header_t firmware_header;
    ArticFirmwareFile firmware_file;

    firmware_file.read((unsigned char *)&firmware_header, sizeof(firmware_header));

	for (uint8_t mem_sel = 0; mem_sel < NUM_FIRMWARE_FILES_ARTIC; mem_sel++)
    {
        uint32_t size = 0;
        uint8_t length_transfer = 0;
        uint8_t rx_buffer[4];
        uint32_t start_address = 0;
        uint8_t write_buffer[MAX_BURST];
        uint32_t bytes_written = 0;
        uint32_t last_address = 0;
        uint32_t bytes_total_read = 0;
        uint32_t address;
        uint32_t data;
        mem_id_t mode;

        mode = order_mode[mem_sel];

        // select number of transfer needed and the size of each transfer
        switch (mode)
        {
            case PMEM:
                size = firmware_header.PMEM_length;
                length_transfer = SIZE_SPI_REG_PMEM;
                break;
            case XMEM:
                size = firmware_header.XMEM_length;
                length_transfer = SIZE_SPI_REG_XMEM_YMEM_IOMEM;
                break;
            case YMEM:
                size = firmware_header.YMEM_length;
                length_transfer = SIZE_SPI_REG_XMEM_YMEM_IOMEM;
                break;
            case INVALID_MEM:
            case IOMEM:
            default:
                break;
        }

        while (bytes_total_read < size)
        {
            // Read 3 bytes of address and 3/4 bytes of data
        	firmware_file.read(buffer, FIRMWARE_ADDRESS_LENGTH + length_transfer);

            // Check next address and next data
            address = 0;
            data = 0;
            std::memcpy(&address, buffer, FIRMWARE_ADDRESS_LENGTH);
            std::memcpy(&data, buffer + FIRMWARE_ADDRESS_LENGTH, length_transfer);

            // Sum bytes read from the file
            bytes_total_read += FIRMWARE_ADDRESS_LENGTH + length_transfer;

            // If there is a memory discontinuity or the buffer is full send the whole buffer
            if (last_address + 1 < address || (bytes_written + length_transfer) >= MAX_BURST)
            {
                // Configure and send the buffer content
                burst_access(mode, start_address, write_buffer, rx_buffer, bytes_written, false);
                start_address = address;
                bytes_written = 0;
            }

            // Copy next data in the buffer
            reverse_memcpy(&(write_buffer[bytes_written]), (uint8_t *) &data, length_transfer);
            last_address = address;
            bytes_written += length_transfer;
            data = 0;
        }

        // If there is data to be sent, (it has to be, otherwise the number of data is multiple of MAX_BURST.
        if (bytes_written > 0)
        {
            // Wait few ms to continue the operations 13 ms, just in case we send very small amount of bytes
            nrf_delay_ms(SAT_ARTIC_DELAY_FINISH_BURST_MS);
            burst_access(mode, start_address, write_buffer, rx_buffer, bytes_written, false);
        }
    }

	// Bring DSP out of reset
    DEBUG_TRACE("ArticTransceiver::send_fw_files: Bringing Artic out of reset");
    send_artic_command(DSP_CONFIG, NULL);

    DEBUG_TRACE("ArticTransceiver::send_fw_files: Waiting for interrupt 1");

    // Interrupt 1 will be high when start-up is complete
    wait_interrupt(SAT_ARTIC_DELAY_INTERRUPT_1_PROG_MS, INTERRUPT_1);

    DEBUG_TRACE("ArticTransceiver::send_fw_files: Artic booted");

    clear_interrupt(INTERRUPT_1);

    DEBUG_TRACE("ArticTransceiver::send_fw_files: Checking CRC values");
    check_crc(&firmware_header);
}

void ArticTransceiver::program_firmware(void)
{
    DEBUG_TRACE("ArticTransceiver::program_firmware: WAIT DSP RESET");

    // Wait until the device's status register contains 85
    // NOTE: This 85 value is undocumentated but can be seen in the supplied Artic_evalboard.py file
    int retries = 3;
    do
    {
        uint32_t artic_response = 0;
        nrf_delay_ms(SAT_ARTIC_DELAY_BOOT_MS);

        try {
        	send_artic_command(DSP_STATUS, &artic_response);
        } catch (int e) {
        	// No action
        }

        DEBUG_TRACE("ArticTransceiver::program_firmware: resp: %lu", artic_response);

        if (artic_response == 85)
            break;
    } while (--retries >= 0);

    if (retries < 0)
    {
        DEBUG_ERROR("ArticTransceiver::program_firmware: BOOT ERROR");
        throw ErrorCode::ARTIC_BOOT_TIMEOUT;
    }

    DEBUG_TRACE("ArticTransceiver::program_firmware: Uploading firmware to ARTIC");
    send_fw_files();
}

void ArticTransceiver::set_tcxo_warmup_time(uint32_t time_s)
{
    uint8_t write_buffer[3];
    write_buffer[0] = (time_s) >> 16;
    write_buffer[1] = (time_s) >> 8;
    write_buffer[2] = (time_s);

    burst_access(XMEM, TCXO_WARMUP_TIME_ADDRESS, write_buffer, NULL, sizeof(write_buffer), false);
}

void ArticTransceiver::get_status_register(uint32_t *status)
{
    uint8_t buffer[SIZE_SPI_REG_XMEM_YMEM_IOMEM];

    burst_access(IOMEM, INTERRUPT_ADDRESS, NULL, buffer, SIZE_SPI_REG_XMEM_YMEM_IOMEM,  true);
    *status = 0;
    reverse_memcpy((uint8_t *) status, buffer, SIZE_SPI_REG_XMEM_YMEM_IOMEM);
}

void ArticTransceiver::power_off()
{
    DEBUG_TRACE("ArticTransceiver::power_off");

	GPIOPins::clear(BSP::GPIO::GPIO_SAT_RESET);
	GPIOPins::clear(BSP::GPIO::GPIO_SAT_EN);

	delete m_nrf_spim;
    m_nrf_spim = nullptr; // Invalidate this pointer so if we call this function again it doesn't call delete on an invalid pointer

	// FIXME: should this be moved into the NrfSPIM driver?
	nrf_gpio_cfg_output(BSP::SPI_Inits[SPI_SATELLITE].spim_config.ss_pin);
	nrf_gpio_pin_clear(BSP::SPI_Inits[SPI_SATELLITE].spim_config.ss_pin);
    nrf_gpio_cfg_input(BSP::SPI_Inits[SPI_SATELLITE].spim_config.mosi_pin, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(BSP::SPI_Inits[SPI_SATELLITE].spim_config.miso_pin, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(BSP::SPI_Inits[SPI_SATELLITE].spim_config.sck_pin, NRF_GPIO_PIN_PULLDOWN);
}

ArticTransceiver::ArticTransceiver() {
    m_nrf_spim = nullptr;
	hardware_init();
}

void ArticTransceiver::power_on()
{
    DEBUG_TRACE("ArticTransceiver::power_on");

	m_nrf_spim = new NrfSPIM(SPI_SATELLITE);

    GPIOPins::set(BSP::GPIO::GPIO_SAT_EN);
    GPIOPins::set(BSP::GPIO::GPIO_SAT_RESET);

    // Wait for the Artic device to power on
    nrf_delay_ms(SAT_ARTIC_DELAY_POWER_ON_MS);

    // Reset the Artic device
    GPIOPins::clear(BSP::GPIO::GPIO_SAT_RESET);

    // Wait few ms to allow the device to detect the reset
    nrf_delay_ms(SAT_ARTIC_DELAY_RESET_MS);

    // Exit reset on the device to enter boot mode
    GPIOPins::set(BSP::GPIO::GPIO_SAT_RESET);

    nrf_delay_ms(SAT_ARTIC_DELAY_RESET_MS);

    // Program firmware
    program_firmware();

    // Set TCXO warm-up time
    set_tcxo_warmup_time(DEFAULT_TCXO_WARMUP_TIME_SECONDS);
}

void ArticTransceiver::send_packet(ArgosPacket const& packet, unsigned int total_bits, const ArgosMode mode)
{
    // Number of tails bits is zero for A2 and non-zero for A3
	unsigned int num_tail_bits = 0;

	switch (mode) {
	default:
	case ArgosMode::ARGOS_2:
		send_command_check_clean(ARTIC_CMD_SET_PTT_A2_TX_MODE, 1, MCU_COMMAND_ACCEPTED, true, SAT_ARTIC_DELAY_INTERRUPT_MS);
		break;
	case ArgosMode::ARGOS_3:
		send_command_check_clean(ARTIC_CMD_SET_PTT_A3_TX_MODE, 1, MCU_COMMAND_ACCEPTED, true, SAT_ARTIC_DELAY_INTERRUPT_MS);
		num_tail_bits = (total_bits >= ARTIC_PTT_A3_248_MAX_USER_BITS) ? ARTIC_PTT_A3_248_MSG_NUM_TAIL_BITS : ARTIC_PTT_A3_120_MSG_NUM_TAIL_BITS;
		break;
	}

	// Overwrite first 24-bit SYNC with the 24-bit total length field (required at the head of the packet data)
	// this length must exclude the 24-bit header itself
	uint8_t *buffer = (uint8_t *)packet.c_str();
	buffer[0] = ((total_bits + num_tail_bits - 24) >> 16);
	buffer[1] = ((total_bits + num_tail_bits - 24) >> 8);
	buffer[2] = (total_bits + num_tail_bits - 24);
    burst_access(XMEM, TX_PAYLOAD_ADDRESS, (const uint8_t *)buffer, NULL, (total_bits / 8), false);

    print_status();

    DEBUG_TRACE("ArticTransceiver::send_packet: sending message");

    // Send to ARTIC the command for sending only one packet and wait for the response TX_FINISHED
    send_command_check_clean(ARTIC_CMD_START_TX_1M_SLEEP, 1, TX_FINISHED, true, SAT_ARTIC_TIMEOUT_SEND_TX_MS);
}

void ArticTransceiver::set_frequency(const double freq) {
	unsigned int fractional_part;
	fractional_part = (unsigned int)std::round((((4.0 * freq * 1E6) / 26E6) - 61.0) * 4194304.0);

    uint8_t write_buffer[3];
    write_buffer[0] = (fractional_part) >> 16;
    write_buffer[1] = (fractional_part) >> 8;
    write_buffer[2] = (fractional_part);

	// Only supports Argos 2 and 3 band
	burst_access(XMEM, TX_FREQUENCY_ARGOS_2_3, write_buffer, NULL, sizeof(write_buffer), false);
}

void ArticTransceiver::set_tx_power(const BaseArgosPower power) {
#ifndef BOARD_GENTRACKER
	(void)power;
#else
	// GA16 and GA8 pins allows PA gain to be set only coarsely as:
	// 0dB   - 00
	// 8dB   - 01
	// 16dB  - 10
	// 24dB  - 11
	GPIOPins::clear(BSP::GPIO::GPIO_G8_33);
	GPIOPins::clear(BSP::GPIO::GPIO_G16_33);
	switch (power) {
	case BaseArgosPower::POWER_40_MW:
		GPIOPins::set(BSP::GPIO::GPIO_G16_33);
		break;
	case BaseArgosPower::POWER_500_MW:
	case BaseArgosPower::POWER_200_MW:
		GPIOPins::set(BSP::GPIO::GPIO_G8_33);
		GPIOPins::set(BSP::GPIO::GPIO_G16_33);
		break;
	case BaseArgosPower::POWER_3_MW:
		GPIOPins::set(BSP::GPIO::GPIO_G8_33);
		break;
	default:
		break;

	}
#endif
}
