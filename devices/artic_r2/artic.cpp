#include <stdint.h>
#include <cstring>

#include "error.hpp"
#include "debug.hpp"
#include "artic.hpp"

#include "artic_firmware.hpp"
#include "artic_registers.hpp"

extern "C" {
#include "syshal_spi.h"
#include "syshal_gpio.h"
};

enum artic_cmd_t
{
    DSP_STATUS,
    DSP_CONFIG,
};

enum mem_id_t
{
    XMEM,
    YMEM,
    PMEM,
    IOMEM,
    INVALID_MEM = 0xFF
};

struct firmware_header_t
{
    uint32_t XMEM_length;
    uint32_t XMEM_CRC;
    uint32_t YMEM_length;
    uint32_t YMEM_CRC;
    uint32_t PMEM_length;
    uint32_t PMEM_CRC;
};


#define SYSHAL_SAT_ARTIC_DELAY_TICK_INTERRUPT_MS 10
#define INVALID_MEM_SELECTION (0xFF)

#define SINGLE_POSITION_DATA_PAYLOAD_LEN_BITS (99)
#define SINGLE_POSITION_BCH_LEN_BITS (21)
#define SINGLE_POSITION_FULL_SIZE_BYTES (24)

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

static int print_status(void);

static constexpr const char *const status_string[] =
{
    "IDLE",                                   //The firmware is idle and ready to accept commands.
    "RX_IN_PROGRESS",                         // The firmware is receiving.
    "TX_IN_PROGRESS",                         // The firmware is transmitting.
    "BUSY",                                   // The firmware is changing state.
    //      Interrupt 1 flags
    "RX_VALID_MESSAGE",                       // A message has been received.
    "RX_SATELLITE_DETECTED",                  // A satellite has been detected.
    "TX_FINISHED"                            // The transmission was completed.
    "MCU_COMMAND_ACCEPTED",                   // The configuration command has been accepted.
    "CRC_CALCULATED",                         // CRC calculation has finished.
    "IDLE_STATE",                             // Firmware returned to the idle state.
    "RX_CALIBRATION_FINISHED",                // RX offset calibration has completed.
    "RESERVED_11",                            //
    "RESERVED_12",                            //
    //      Interrupt 2 flags
    "RX_TIMEOUT",                             // The specified reception time has been exceeded.
    "SATELLITE_TIMEOUT",                      // No satellite was detected within the specified time.
    "RX_BUFFER_OVERFLOW",                     // A received message is lost. No buffer space left.
    "TX_INVALID_MESSAGE",                     // Incorrect TX payload length specified.
    "MCU_COMMAND_REJECTED",                   // Incorrect command send or Firmware is not in idle.
    "MCU_COMMAND_OVERFLOW",                   // Previous command was not yet processed.
    "RESERVED_19",                            //
    "RESERVER_20",                            //
    // Others
    "INTERNAL_ERROR",                         // An internal error has occurred.
    "dsp2mcu_int1",                           // Interrupt 1 pin status
    "dsp2mcu_int2",
};

static inline uint8_t convert_mem_sel(mem_id_t mode)
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
        default:
            return INVALID_MEM_SELECTION;
            break;
    }
}

static inline mem_id_t convert_mode(uint8_t mem_sel)
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

static void send_artic_command(artic_cmd_t cmd, uint32_t *response)
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

    int ret = syshal_spi_transfer(SPI_SATALLITE, buffer_tx, buffer_rx, 4);
    if (ret) {
    	throw ErrorCode:SPI_COMMS_ERROR;
    }
    if (response)
        *response = (buffer_rx[1] << 16) | (buffer_rx[2] << 8) | buffer_rx[3];
}


static void check_crc(firmware_header_t *firmware_header)
{
    uint32_t crc;
    uint8_t crc_buffer[NUM_FIRMWARE_FILES_ARTIC * SIZE_SPI_REG_XMEM_YMEM_IOMEM]; // 3 bytes CRC for each firmware file
    int ret;

    spi_read(XMEM, CRC_ADDRESS, crc_buffer, NUM_FIRMWARE_FILES_ARTIC * SIZE_SPI_REG_XMEM_YMEM_IOMEM ); // 3 bytes CRC for each firmware local_file_id

    // Check CRC Value PMEM
    crc = 0;
    reverse_memcpy((uint8_t *)&crc, &crc_buffer[0], SIZE_SPI_REG_XMEM_YMEM_IOMEM);
    if (firmware_header.PMEM_CRC != crc)
    {
        DEBUG_ERROR("PMEM CRC 0x%08lX DOESN'T MATCH EXPECTED 0x%08lX", crc, firmware_header.PMEM_CRC);
        throw ErrorCode::ARTIC_CRC_FAILURE;
    }

    // Check CRC Value XMEM
    crc = 0;
    reverse_memcpy((uint8_t *)&crc, &crc_buffer[3], SIZE_SPI_REG_XMEM_YMEM_IOMEM);
    if (firmware_header.XMEM_CRC != crc)
    {
        DEBUG_ERROR("XMEM CRC 0x%08lX DOESN'T MATCH EXPECTED 0x%08lX", crc, firmware_header.XMEM_CRC);
        throw ErrorCode::ARTIC_CRC_FAILURE;
    }

    // Check CRC Value YMEM
    crc = 0;
    reverse_memcpy((uint8_t *)&crc, &crc_buffer[6], SIZE_SPI_REG_XMEM_YMEM_IOMEM);
    if (firmware_header.YMEM_CRC != crc)
    {
        DEBUG_ERROR("YMEM CRC 0x%08lX DOESN'T MATCH EXPECTED 0x%08lX", crc, firmware_header.YMEM_CRC);
        throw ErrorCode::ARTIC_CRC_FAILURE;
    }
}

static void configure_burst(mem_id_t mode, bool read, uint32_t start_address)
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

    ret = syshal_spi_transfer(SPI_SATALLITE, buffer_tx, buffer_rx, 4);
    if (ret) {
        throw ErrorCode::SPI_COMMS_ERROR;
    }

    syshal_time_delay_ms(SYSHAL_SAT_ARTIC_DELAY_SET_BURST);
}

static void burst_access(mem_id_t mode, uint32_t start_address, const uint8_t *tx_data, uint8_t *rx_data, size_t size, bool read )
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
    syshal_spi_finish_transfer(SPI_SATALLITE);
    syshal_time_delay_ms(SYSHAL_SAT_ARTIC_DELAY_FINISH_BURST);
}

static void send_burst(const uint8_t *tx_data, uint8_t *rx_data, size_t size, uint8_t length_transfer, bool read)
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
            ret = syshal_spi_transfer_continous(SPI_SATALLITE, buffer, &rx_data[i * length_transfer], length_transfer);
            if (ret) {
                throw ErrorCode::SPI_COMMS_ERROR;
            }

            delay_count += length_transfer;
            syshal_time_delay_us(SYSHAL_SAT_ARTIC_DELAY_TRANSFER);
            if (delay_count > NUM_BYTES_BEFORE_WAIT)
            {
                delay_count = 0;
                syshal_time_delay_ms(SYSHAL_SAT_ARTIC_DELAY_BURST);
            }

        }
    }
    else
    {
        for (uint32_t i = 0; i < num_transfer; ++i)
        {
            ret = syshal_spi_transfer_continous(SPI_SATALLITE, &tx_data[i * length_transfer], buffer, length_transfer);
            if (ret) {
                throw ErrorCode::SPI_COMMS_ERROR;
            }
            delay_count += length_transfer;
            syshal_time_delay_us(SYSHAL_SAT_ARTIC_DELAY_TRANSFER);
            if (delay_count > NUM_BYTES_BEFORE_WAIT)
            {
                delay_count = 0;
                syshal_time_delay_ms(SYSHAL_SAT_ARTIC_DELAY_BURST);
            }
        }
    }
}

static void spi_read(mem_id_t mode, uint32_t start_address, uint8_t *buffer_read, size_t size)
{
    std::memset(buffer_read, 0, size);
    burst_access(mode, start_address, NULL, buffer_read, size, true);
}

static void print_status(void)
{
    uint32_t status;
    get_status_register(&status);

    for (int i = 0; i < TOTAL_NUMBER_STATUS_FLAG; ++i)
    {
        if (status & (1 << i))
            DEBUG_TRACE("%s", (char *)status_string[i]);
    }
}

static void clear_interrupt(uint8_t interrupt_num)
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

static void send_command(uint8_t command)
{
    uint8_t buffer_read;
    int ret;

    ret = syshal_spi_transfer(SPI_SATALLITE, &command, &buffer_read, sizeof(command));
    if (ret) {
        throw ErrorCode::SPI_COMMS_ERROR;
    }
}

static void wait_interrupt(uint32_t timeout, uint8_t interrupt_num)
{
    uint32_t init_time;
    uint32_t time_running = 0;
    uint8_t gpio_port;
    bool int_status;

    if (interrupt_num == INTERRUPT_1)
        gpio_port = SYSHAL_SAT_GPIO_INT_1;
    else
        gpio_port = SYSHAL_SAT_GPIO_INT_2;

    init_time = syshal_time_get_ticks_ms();
    do
    {
        syshal_time_delay_ms(SYSHAL_SAT_ARTIC_DELAY_TICK_INTERRUPT_MS);
        time_running =  syshal_time_get_ticks_ms() - init_time;
        int_status = syshal_gpio_get_input(gpio_port);
    }
    while (time_running < timeout && !int_status);

    if (int_status == false)
    {
        DEBUG_TRACE("Waiting for interrupt_%u timed out", interrupt_num);
        throw ErrorCode::ARTIC_IRQ_TIMEOUT;
    }
}

static void send_command_check_clean(uint8_t command, uint8_t interrupt_number, uint8_t status_flag_number, bool value, uint32_t interrupt_timeout)
{
    uint32_t status = 0;

    clear_interrupt(interrupt_number);
    send_command(command);
    wait_interrupt(interrupt_timeout, interrupt_number);
    get_status_register(&status);

    if ((status & (1 << status_flag_number)) == !value) {
    	throw ErrorCode::ARTIC_INCORRECT_STATUS;
    }

    clear_interrupt(interrupt_number);
}

void hardware_init()
{
    syshal_gpio_init(SYSHAL_SAT_GPIO_RESET);
    syshal_gpio_init(SYSHAL_SAT_GPIO_INT_1);
    syshal_gpio_init(SYSHAL_SAT_GPIO_INT_2);
    syshal_gpio_init(SYSHAL_SAT_GPIO_POWER_ON);
    syshal_gpio_init(GPIO_G8_33);
    syshal_gpio_init(GPIO_G16_33);

    syshal_gpio_set_output_low(SYSHAL_SAT_GPIO_RESET);
    syshal_gpio_set_output_low(SYSHAL_SAT_GPIO_POWER_ON);

    syshal_gpio_set_output_low(GPIO_G8_33);
    syshal_gpio_set_output_low(GPIO_G16_33);
}

static void send_fw_files(void)
{
    uint8_t buffer[MAXIMUM_READ_FIRMWARE_OPERATION];
    uint32_t bytes_read;
    mem_id_t order_mode[NUM_FIRMWARE_FILES_ARTIC] = {XMEM, YMEM, PMEM};
    firmware_header_t firmware_header;
    ArticFirmwareFile firmware_file;

    firmware_file.read((unsigned char *)&firmware_header, sizeof(firmware_header));

	for (uint8_t mem_sel = 0; mem_sel < NUM_FIRMWARE_FILES_ARTIC; mem_sel++)
    {
        int ret;
        uint32_t size;
        uint8_t length_transfer;
        uint8_t rx_buffer[4];
        uint32_t start_address = 0;
        uint8_t *write_buffer;
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
                size = firmware_header->PMEM_length;
                length_transfer = SIZE_SPI_REG_PMEM;
                break;
            case XMEM:
                size = firmware_header->XMEM_length;
                length_transfer = SIZE_SPI_REG_XMEM_YMEM_IOMEM;
                break;
            case YMEM:
                size = firmware_header->YMEM_length;
                length_transfer = SIZE_SPI_REG_XMEM_YMEM_IOMEM;
                break;
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
            syshal_time_delay_ms(SYSHAL_SAT_ARTIC_DELAY_FINISH_BURST);
            burst_access(mode, start_address, write_buffer, rx_buffer, bytes_written, false);
        }
    }

	// Bring DSP out of reset
    DEBUG_TRACE("Bringing Artic out of reset");
    send_artic_command(DSP_CONFIG, NULL);

    DEBUG_TRACE("Waiting for interrupt 1");

    // Interrupt 1 will be high when start-up is complete
    wait_interrupt(SYSHAL_SAT_ARTIC_DELAY_INTERRUPT_1_PROG, INTERRUPT_1);

    DEBUG_TRACE("Artic booted");

    clear_interrupt(INTERRUPT_1);

    DEBUG_TRACE("Checking CRC values");
    check_crc(&firmware_header);
}

static void program_firmware(void)
{
    DEBUG_TRACE("WAIT DSP IN RESET");

    // Wait until the device's status register contains 85
    // NOTE: This 85 value is undocumentated but can be seen in the supplied Artic_evalboard.py file
    int retries = SYSHAL_SAT_MAX_RETRIES;
    do
    {
        uint32_t artic_response;
        syshal_time_delay_ms(SYSHAL_SAT_ARTIC_DELAY_BOOT);

        try {
        	send_artic_command(DSP_STATUS, &artic_response);
        } catch (int e) {
        	// No action
        }

        DEBUG_TRACE("Artic resp: %lu", artic_response);

        if (artic_response == 85)
            break;
    } while (--retries >= 0);

    if (retries < 0)
    {
        DEBUG_ERROR("ARTIC ERROR BOOT");
        throw ErrorCode::ARTIC_BOOT_TIMEOUT;
    }

    DEBUG_TRACE("Uploading firmware to ARTIC");
    send_fw_files();
}

static void set_tcxo_warmup_time(uint8_t time_s)
{
    burst_access(XMEM, TCXO_WARMUP_TIME_ADDRESS, &time_s, NULL, sizeof(time_s), false);
}

static void get_status_register(uint32_t *status)
{
    int ret;
    uint8_t buffer[SIZE_SPI_REG_XMEM_YMEM_IOMEM];

    burst_access(IOMEM, INTERRUPT_ADDRESS, NULL, buffer, SIZE_SPI_REG_XMEM_YMEM_IOMEM,  true);
    *status = 0;
    reverse_memcpy((uint8_t *) status, buffer, SIZE_SPI_REG_XMEM_YMEM_IOMEM);
}

void ArticTransceiver::power_off() {

    syshal_gpio_set_output_low(SYSHAL_SAT_GPIO_RESET);
    syshal_gpio_set_output_low(SYSHAL_SAT_GPIO_POWER_ON);
    syshal_gpio_set_output_low(GPIO_G8_33);
    syshal_gpio_set_output_low(GPIO_G16_33);

    syshal_spi_term(SPI_SATALLITE);

	nrf_gpio_cfg_output(SPI_Inits[SPI_SATALLITE].spim_config.ss_pin);
	nrf_gpio_pin_clear(SPI_Inits[SPI_SATALLITE].spim_config.ss_pin);
    nrf_gpio_cfg_input(SPI_Inits[SPI_SATALLITE].spim_config.mosi_pin, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(SPI_Inits[SPI_SATALLITE].spim_config.miso_pin, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(SPI_Inits[SPI_SATALLITE].spim_config.sck_pin, NRF_GPIO_PIN_PULLDOWN);
}

void ArticTransceiver::power_on()
{
	// Power on device
    syshal_gpio_set_output_high(GPIO_G8_33);
    syshal_gpio_set_output_high(GPIO_G16_33);
    syshal_spi_init(SPI_SATALLITE);

    syshal_gpio_set_output_high(SYSHAL_SAT_GPIO_POWER_ON);
    syshal_gpio_set_output_high(SYSHAL_SAT_GPIO_RESET);

    // Wait for the Artic device to power on
    syshal_time_delay_ms(SYSHAL_SAT_ARTIC_DELAY_POWER_ON);

    // Reset the Artic device
    syshal_gpio_set_output_low(SYSHAL_SAT_GPIO_RESET);

    // Wait few ms to allow the device to detect the reset
    syshal_time_delay_ms(SYSHAL_SAT_ARTIC_DELAY_RESET);

    // Exit reset on the device to enter boot mode
    syshal_gpio_set_output_high(SYSHAL_SAT_GPIO_RESET);

    syshal_time_delay_ms(SYSHAL_SAT_ARTIC_DELAY_RESET);

    // Program firmware
    program_firmware();
}

void ArticTransceiver::send_packet(ArgosPacket const& packet, unsigned int total_bits)
{
    // Set ARGOS TX MODE in ARTIC device and wait for the status response
    send_command_check_clean(ARTIC_CMD_SET_PTT_A2_TX_MODE, 1, MCU_COMMAND_ACCEPTED, true, SAT_ARTIC_DELAY_INTERRUPT);

    // It could be a problem if we set less data than we are already sending, just be careful and in case change TOTAL
    // Burst transfer the tx payload
    burst_access(XMEM, TX_PAYLOAD_ADDRESS, packet, NULL, total_bits, false);

#ifndef DEBUG_DISABLED
    print_status();
#endif

    // Send to ARTIC the command for sending only one packet and wait for the response TX_FINISHED
    send_command_check_clean(ARTIC_CMD_START_TX_1M_SLEEP, 1, TX_FINISHED, true, SAT_ARTIC_TIMEOUT_SEND_TX);

    // If there is a problem wait until interrupt 2 is launched and get the status response
    wait_interrupt(SYSHAL_SAT_ARTIC_DELAY_INTERRUPT, INTERRUPT_2);
    clear_interrupt(INTERRUPT_2);
}

void ArticTransceiver::set_frequency(const double freq) {
	(void)freq;
	// TODO
}

void ArticTransceiver::set_tx_power(const BaseArgosPower power) {
	(void)power;
	// TODO
}
