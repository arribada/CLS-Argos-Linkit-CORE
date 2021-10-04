#include <stdint.h>
#include <cstring>
#include <cmath>

#include "bsp.hpp"
#include "artic_registers.hpp"
#include "error.hpp"
#include "debug.hpp"
#include "artic_firmware.hpp"
#include "artic.hpp"
#include "gpio.hpp"
#include "scheduler.hpp"
#include "nrf_irq.hpp"
#include "binascii.hpp"
#include "crc16.hpp"

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

extern Scheduler *system_scheduler;


static_assert(ARRAY_SIZE(status_string) == TOTAL_NUMBER_STATUS_FLAG);


static void run_until(bool& stopped, std::function<bool()> func, std::function<void()> on_complete, const char *task_name, unsigned int delay_ms) {
	system_scheduler->post_task_prio([&stopped, func, on_complete, task_name, delay_ms]() {
		if (func() == false) {
			if (!stopped)
				run_until(stopped, func, on_complete, task_name, delay_ms);
		}
		else
			on_complete();
	}, task_name, Scheduler::DEFAULT_PRIORITY, delay_ms);
}


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
        m_notification_callback(ArgosAsyncEvent::ERROR);
        return;
        //throw ErrorCode::ARTIC_CRC_FAILURE;
    }

    // Check CRC Value XMEM
    crc = 0;
    reverse_memcpy((uint8_t *)&crc, &crc_buffer[3], SIZE_SPI_REG_XMEM_YMEM_IOMEM);
    if (firmware_header->XMEM_CRC != crc)
    {
        DEBUG_ERROR("ArticTransceiver::check_crc: XMEM CRC 0x%08lX DOESN'T MATCH EXPECTED 0x%08lX", crc, firmware_header->XMEM_CRC);
        m_notification_callback(ArgosAsyncEvent::ERROR);
        return;
        //throw ErrorCode::ARTIC_CRC_FAILURE;
    }

    // Check CRC Value YMEM
    crc = 0;
    reverse_memcpy((uint8_t *)&crc, &crc_buffer[6], SIZE_SPI_REG_XMEM_YMEM_IOMEM);
    if (firmware_header->YMEM_CRC != crc)
    {
        DEBUG_ERROR("ArticTransceiver::check_crc: YMEM CRC 0x%08lX DOESN'T MATCH EXPECTED 0x%08lX", crc, firmware_header->YMEM_CRC);
        m_notification_callback(ArgosAsyncEvent::ERROR);
        return;
        //throw ErrorCode::ARTIC_CRC_FAILURE;
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

            // LW: This delay appears to be necessary -- removing it causes
            // CRC verification errors during FW upload
            nrf_delay_us(SAT_ARTIC_DELAY_TRANSFER_US);
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
            // LW: This delay appears to be necessary -- removing it causes
            // CRC verification errors during FW upload
            nrf_delay_us(SAT_ARTIC_DELAY_TRANSFER_US);
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
        case INTERRUPT_1:
            send_command(ARTIC_CMD_CLEAR_INT1);
            break;
        case INTERRUPT_2:
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
    bool int_status;

    uint32_t iterations = timeout_ms / SAT_ARTIC_DELAY_TICK_INTERRUPT_MS;

    do
    {
        nrf_delay_ms(SAT_ARTIC_DELAY_TICK_INTERRUPT_MS);
        int_status = m_irq_int[interrupt_num]->poll();
    }
    while (iterations-- && !int_status);

    if (!int_status)
    {
        DEBUG_TRACE("ArticTransceiver::wait_interrupt: Waiting for interrupt_%u timed out", interrupt_num + 1);
        throw ErrorCode::ARTIC_IRQ_TIMEOUT;
    }
}

void ArticTransceiver::send_command_check_clean(uint8_t command, uint8_t interrupt_number, uint8_t status_flag_number, uint32_t interrupt_timeout_ms)
{
    uint32_t status = 0;

    clear_interrupt(interrupt_number);
    send_command(command);
    wait_interrupt(interrupt_timeout_ms, interrupt_number);
    get_status_register(&status);

    if ((status & (1 << status_flag_number)) == 0) {
    	m_notification_callback(ArgosAsyncEvent::ERROR);
    	//throw ErrorCode::ARTIC_INCORRECT_STATUS;
    }

    clear_interrupt(interrupt_number);
}

void ArticTransceiver::send_command_check_clean_async(uint8_t command, uint8_t interrupt_number, uint8_t status_flag_number,
		uint32_t interrupt_timeout_ms,
		std::function<void()> on_success)
{
    clear_interrupt(interrupt_number);
    send_command(command);

    m_timeout_task = system_scheduler->post_task_prio([this, interrupt_number]() {
        DEBUG_ERROR("ArticTransceiver::wait_interrupt: Waiting for interrupt_%u timed out", interrupt_number + 1);
        m_irq_int[interrupt_number]->disable();
        m_notification_callback(ArgosAsyncEvent::ERROR);
        //throw ErrorCode::ARTIC_IRQ_TIMEOUT;
    }, "CommandTimeoutTask", Scheduler::DEFAULT_PRIORITY, interrupt_timeout_ms);

    m_irq_int[interrupt_number]->enable([this, interrupt_number, status_flag_number, on_success]() {

    	system_scheduler->cancel_task(m_timeout_task);

    	uint32_t status = 0;
        get_status_register(&status);
        clear_interrupt(interrupt_number);
        m_irq_int[interrupt_number]->disable();

        if ((status & (1 << status_flag_number)) == 0) {
            m_notification_callback(ArgosAsyncEvent::ERROR);
        	//throw ErrorCode::ARTIC_INCORRECT_STATUS;
            return;
        }

        on_success();
    });
}

void ArticTransceiver::hardware_init()
{
	GPIOPins::clear(BSP::GPIO::GPIO_SAT_RESET);
	GPIOPins::clear(BSP::GPIO::GPIO_SAT_EN);
    m_is_powered_on = false;
    m_is_rx_enabled = false;
}

void ArticTransceiver::send_fw_files(std::function<void()> on_success)
{
    m_mem_sel = 0;
    m_bytes_total_read = 0;

    // Reset firmware file read position to start before reading header
    m_firmware_file.seek(0);
    m_firmware_file.read((unsigned char *)&m_firmware_header, sizeof(m_firmware_header));

    run_until(m_deferred_task_stopped, [this](){

        // First transfer of the section?
        if (m_bytes_total_read == 0) {
            static const mem_id_t order_mode[NUM_FIRMWARE_FILES_ARTIC] = {XMEM, YMEM, PMEM};
            m_mode = order_mode[m_mem_sel];

            // Reset counters
            m_bytes_pending = 0;
            m_last_address = 0;
            m_start_address = 0;

			// Select number of transfer needed and the size of next section to transfer
			switch (m_mode)
			{
				case PMEM:
					m_size = m_firmware_header.PMEM_length;
					m_length_transfer = SIZE_SPI_REG_PMEM;
					break;
				case XMEM:
					m_size = m_firmware_header.XMEM_length;
					m_length_transfer = SIZE_SPI_REG_XMEM_YMEM_IOMEM;
					break;
				case YMEM:
					m_size = m_firmware_header.YMEM_length;
					m_length_transfer = SIZE_SPI_REG_XMEM_YMEM_IOMEM;
					break;
				case INVALID_MEM:
				case IOMEM:
				default:
					break;
			}
        }

    	// Prepare next chunk to program
        while (m_bytes_total_read < m_size)
        {
            uint32_t address = 0;
            uint32_t data = 0;
            uint8_t buffer[MAXIMUM_READ_FIRMWARE_OPERATION];

            // Read 3 bytes of address and 3/4 bytes of data
            m_firmware_file.read(buffer, FIRMWARE_ADDRESS_LENGTH + m_length_transfer);

            // Check next address and next data
            std::memcpy(&address, buffer, FIRMWARE_ADDRESS_LENGTH);
            std::memcpy(&data, buffer + FIRMWARE_ADDRESS_LENGTH, m_length_transfer);

            // Sum bytes read from the file
            m_bytes_total_read += FIRMWARE_ADDRESS_LENGTH + m_length_transfer;

			//DEBUG_TRACE("ArticTransceiver::send_fw_files: mem_sel=%u pending=%u last_address=%06x address=%06x total_read=%u size=%u", m_mem_sel, m_bytes_pending, m_last_address, address, m_bytes_total_read, m_size);

            // If there is a memory discontinuity or the buffer is full send the buffer
            if ((m_last_address + 1) < address || (m_bytes_pending + m_length_transfer) >= MAX_BURST)
            {
                // Configure and send the buffer content, clear pending count
    			//DEBUG_TRACE("ArticTransceiver::send_fw_files: burst_access: mode=%u start_address=%06x pending=%u", m_mode, m_start_address, m_bytes_pending);
                burst_access(m_mode, m_start_address, m_pending_buffer, nullptr, m_bytes_pending, false);
                m_start_address = address;
                m_bytes_pending = 0;

                // Copy next data into the pending buffer
                reverse_memcpy(&(m_pending_buffer[m_bytes_pending]), (uint8_t *) &data, m_length_transfer);
                m_last_address = address;
                m_bytes_pending = m_length_transfer;

                return false; // Reschedule
            }

            // Copy next data into the pending buffer
            reverse_memcpy(&(m_pending_buffer[m_bytes_pending]), (uint8_t *) &data, m_length_transfer);
            m_last_address = address;
            m_bytes_pending += m_length_transfer;
        }

        // If there is data pending to be sent then send it before moving to the next section
		if (m_bytes_pending > 0)
		{
			//DEBUG_TRACE("ArticTransceiver::send_fw_files: burst_access: mode=%u start_address=%06x pending=%u", m_mode, m_start_address, m_bytes_pending);

			burst_access(m_mode, m_start_address, m_pending_buffer, nullptr, m_bytes_pending, false);

			// Select next file
			m_bytes_total_read = 0;
			m_mem_sel++;
        }

        // Continue running until we've programmed all files
        return (m_mem_sel >= NUM_FIRMWARE_FILES_ARTIC);

    },
	[this, on_success](){

    	// FIXME: make this async

    	// Bring DSP out of reset
        DEBUG_TRACE("ArticTransceiver::send_fw_files: Bringing Artic out of reset");
        send_artic_command(DSP_CONFIG, NULL);

        DEBUG_TRACE("ArticTransceiver::send_fw_files: Waiting for interrupt 1");

        // Interrupt 1 will be high when start-up is complete
        wait_interrupt(SAT_ARTIC_DELAY_INTERRUPT_1_PROG_MS, INTERRUPT_1);

        DEBUG_TRACE("ArticTransceiver::send_fw_files: Artic booted");

        clear_interrupt(INTERRUPT_1);

        DEBUG_TRACE("ArticTransceiver::send_fw_files: Checking CRC values");
        check_crc(&m_firmware_header);

        on_success();

    }, "SendFirmwareChunk", 0);
}

void ArticTransceiver::program_firmware(std::function<void()> on_success)
{
    DEBUG_TRACE("ArticTransceiver::program_firmware: WAIT DSP RESET");

    // Wait until the device's status register contains 0x55
    // NOTE: This 0x55 value is undocumented but can be seen in the supplied Artic_evalboard.py file
    int retries = 3;

    run_until(m_deferred_task_stopped, [this, &retries]() -> bool {

    	if (--retries < 0)
        {
            DEBUG_ERROR("ArticTransceiver::program_firmware: BOOT ERROR");
            m_notification_callback(ArgosAsyncEvent::ERROR);
            return false;
        } else {
            uint32_t artic_response = 0;
			try {
				send_artic_command(DSP_STATUS, &artic_response);
			} catch (int e) {
				// No action
			}
			return (artic_response == 0x55);
        }
    	return false;
    },
	[this, on_success]() {
		send_fw_files(on_success);
    }, "DspReadyForFirmwareProgramming", SAT_ARTIC_DELAY_BOOT_MS);
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

void ArticTransceiver::print_firmware_version()
{
    char version[9];
    memset(version, 0x00, sizeof(version));

    spi_read(PMEM, FIRMWARE_VERSION_ADDRESS, (uint8_t *) version, 8);
    version[8] = '\0';
    DEBUG_TRACE("ArticTransceiver::print_firmware_version: %s", version);
}

void ArticTransceiver::power_off()
{
    DEBUG_TRACE("ArticTransceiver::power_off");

    system_scheduler->cancel_task(m_rx_timeout_task);

    m_deferred_task_stopped = true;

	GPIOPins::clear(BSP::GPIO::GPIO_SAT_RESET);
	GPIOPins::clear(BSP::GPIO::GPIO_SAT_EN);

	delete m_nrf_spim;
    m_nrf_spim = nullptr; // Invalidate this pointer so if we call this function again it doesn't call delete on an invalid pointer

    delete m_irq_int[0];
    m_irq_int[0] = nullptr;

    delete m_irq_int[1];
    m_irq_int[1] = nullptr;

	// FIXME: should this be moved into the NrfSPIM driver?
	nrf_gpio_cfg_output(BSP::SPI_Inits[SPI_SATELLITE].config.ss_pin);
	nrf_gpio_pin_clear(BSP::SPI_Inits[SPI_SATELLITE].config.ss_pin);
    nrf_gpio_cfg_input(BSP::SPI_Inits[SPI_SATELLITE].config.mosi_pin, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(BSP::SPI_Inits[SPI_SATELLITE].config.miso_pin, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(BSP::SPI_Inits[SPI_SATELLITE].config.sck_pin, NRF_GPIO_PIN_PULLDOWN);

    // Mark device as powered off
    m_is_powered_on = false;
    m_is_rx_enabled = false;
}

void ArticTransceiver::add_rx_packet_filter(const uint32_t address) {

	// Read current filter entry count
	uint32_t tmp = 0, lut_size = 0, value = 0;
    burst_access(XMEM, RX_FILTERING_CONFIG + 3, nullptr, (uint8_t *)&tmp, 3, true);
    reverse_memcpy((uint8_t *)&lut_size, (const uint8_t *)&tmp, 3);

    // Add new LUT entry to end of table (24 LSBs first)
    value = address & 0xFFFFFF;
    reverse_memcpy((uint8_t *)&tmp, (const uint8_t *)&value, 3);
    burst_access(XMEM, RX_FILTERING_CONFIG + 4 + (2*lut_size), (const uint8_t *)&tmp, nullptr, 3, false);

    // Add new LUT entry to end of table (4 MSBs last)
    value = (address >> 24) & 0xF;
    reverse_memcpy((uint8_t *)&tmp, (const uint8_t *)&value, 3);
    burst_access(XMEM, RX_FILTERING_CONFIG + 5 + (2*lut_size), (const uint8_t *)&tmp, nullptr, 3, false);

    // Increment the lut_size by 1 and write back to reflect new size
    lut_size++;
    reverse_memcpy((uint8_t *)&tmp, (const uint8_t *)&lut_size, 3);
    burst_access(XMEM, RX_FILTERING_CONFIG + 3, (const uint8_t *)&tmp, nullptr, 3, false);
}


ArticTransceiver::ArticTransceiver() {
    m_nrf_spim = nullptr;
    m_irq_int[0] = nullptr;
    m_irq_int[1] = nullptr;
	hardware_init();
}

bool ArticTransceiver::is_powered_on() {
    return m_is_powered_on;
}

bool ArticTransceiver::is_rx_enabled() {
    return m_is_rx_enabled;
}

void ArticTransceiver::power_on(std::function<void(ArgosAsyncEvent)> notification_callback)
{
    DEBUG_TRACE("ArticTransceiver::power_on");

    m_notification_callback = notification_callback;
    m_deferred_task_stopped = false;

    DEBUG_TRACE("ArticTransceiver::power_on: NrfSPIM()");

    if (!m_nrf_spim)
	    m_nrf_spim = new NrfSPIM(SPI_SATELLITE);

    DEBUG_TRACE("ArticTransceiver::power_on: NrfIRQ(0)");

    if (!m_irq_int[0])
    	m_irq_int[0] = new NrfIRQ(BSP::GPIO::GPIO_INT1_SAT);

    DEBUG_TRACE("ArticTransceiver::power_on: NrfIRQ(1)");

    if (!m_irq_int[1])
    	m_irq_int[1] = new NrfIRQ(BSP::GPIO::GPIO_INT2_SAT);

    GPIOPins::set(BSP::GPIO::GPIO_SAT_EN);
    GPIOPins::set(BSP::GPIO::GPIO_SAT_RESET);

    // Mark the device as powered on
    m_is_powered_on = true;

    system_scheduler->post_task_prio([this](){
        // Reset the Artic device
        GPIOPins::clear(BSP::GPIO::GPIO_SAT_RESET);

        system_scheduler->post_task_prio([this](){
            // Exit reset
        	GPIOPins::set(BSP::GPIO::GPIO_SAT_RESET);
        	// Wait until programming firmware
            system_scheduler->post_task_prio([this](){
                program_firmware([this]() {
					print_firmware_version();
					set_tcxo_warmup_time(DEFAULT_TCXO_WARMUP_TIME_SECONDS);
					m_notification_callback(ArgosAsyncEvent::DEVICE_READY);
                });
            }, "FirmwareProgrammingDelay", Scheduler::DEFAULT_PRIORITY, SAT_ARTIC_DELAY_RESET_MS);
        }, "ArcticResetDelay", Scheduler::DEFAULT_PRIORITY, SAT_ARTIC_DELAY_RESET_MS);
    }, "ArcticPowerOnDelay", Scheduler::DEFAULT_PRIORITY, SAT_ARTIC_DELAY_POWER_ON_MS);
}

void ArticTransceiver::read_packet(ArgosPacket& packet, unsigned int& size) {
	packet = m_rx_packet;
	size = m_rx_packet_bits;
}

void ArticTransceiver::set_idle() {
	// Configure idle mode and disable interrupts
    m_irq_int[INTERRUPT_1]->disable();
	send_command(ARTIC_CMD_SLEEP);
}

bool ArticTransceiver::buffer_rx_packet() {
	uint8_t buffer[MAX_RX_SIZE_BYTES];

	// Read the entire RX payload from X memory
	burst_access(XMEM, RX_PAYLOAD_ADDRESS, nullptr, buffer, MAX_RX_SIZE_BYTES, true);

	// First 24-bit is the packet size (MSB first)
	m_rx_packet_bits = buffer[0] << 16 |
			           buffer[1] << 8 |
					   buffer[2];

	// Make sure the size is valid and does not exceed the maximum allowed size
	if (m_rx_packet_bits >= (8 * MIN_RX_SIZE_BYTES) && m_rx_packet_bits <= (8 * (MAX_RX_SIZE_BYTES-3))) {
		// Assign payload to the locally stored RX buffer
		m_rx_packet.assign((const char *)&buffer[3], (m_rx_packet_bits + 7) / 8);
		DEBUG_TRACE("ArticTransceiver::buffer_rx_packet: data=%s", Binascii::hexlify(m_rx_packet).c_str());

		// Compute CRC16 on the payload
		unsigned int crc = CRC16::checksum(m_rx_packet, m_rx_packet_bits);
		if (crc != 0) {
			DEBUG_TRACE("ArticTransceiver::buffer_rx_packet: discarded packet owing to bad CRC");
			return false;
		}

		return true;
	}

	DEBUG_TRACE("ArticTransceiver::buffer_rx_packet: discarded owing to illegal size %u bits", m_rx_packet_bits);

	// Invalid contents, clear current packet
	m_rx_packet_bits = 0;
	m_rx_packet.clear();

	return false;
}

void ArticTransceiver::set_rx_mode(const ArgosMode mode, const unsigned int timeout_ms) {

	DEBUG_TRACE("ArticTransceiver::set_rx_mode(%u,%u)", (unsigned int)mode, timeout_ms);

	if (m_is_rx_enabled)
		return;

	// Add new entries for filtering on new packet types added since the firmware
	// image was released
	add_rx_packet_filter(0x000005F); // Constellation Satellite Status - format B
	add_rx_packet_filter(0x00000D4); // Satellite Orbit Parameters – format B for ANGELS

	// Mark RX as enabled
	m_is_rx_enabled = true;

	// (Re-)Start RX_TIMEOUT in software
	system_scheduler->cancel_task(m_rx_timeout_task);
	m_rx_timeout_task = system_scheduler->post_task_prio([this]() {
		m_notification_callback(ArgosAsyncEvent::RX_TIMEOUT);
	}, "RXTimeoutTask", Scheduler::DEFAULT_PRIORITY, timeout_ms);

	// Send asynchronous command to configure RX mode and start continuous RX
	send_command_check_clean_async((mode == ArgosMode::ARGOS_3) ? ARTIC_CMD_SET_ARGOS_3_RX_MODE : ARTIC_CMD_SET_ARGOS_4_RX_MODE,
			INTERRUPT_1, MCU_COMMAND_ACCEPTED, SAT_ARTIC_DELAY_INTERRUPT_MS,
	[this]() {
		// Enable interrupt 1 to inform us whenever a packet is available
	    m_irq_int[INTERRUPT_1]->enable([this]() {
	    	uint32_t status = 0;
	        get_status_register(&status);

	        DEBUG_TRACE("ArticTransceiver::set_rx_mode: IRQ status=%08x", status);

	        // Check for valid RX message
	        if (status & (1 << RX_VALID_MESSAGE)) {
	        	// Only notify if new RX packet has been stored
	        	if (buffer_rx_packet())
	        		m_notification_callback(ArgosAsyncEvent::RX_PACKET);
	        }

	        // Don't clear the interrupt until X memory has been read
	        clear_interrupt(INTERRUPT_1);
	    });

		// Configure RX continuous mode
		send_command(ARTIC_CMD_START_RX_CONT);
	});
}

void ArticTransceiver::send_packet(ArgosPacket const& user_payload, unsigned int argos_id, unsigned int payload_length, const ArgosMode mode)
{
	ArgosPacket packet;
	unsigned int total_bits;
	unsigned int stuffing_bits = 0;

	// Only A2/A3 mode is supported
	if ((payload_length + 8) & 31) {
		// Stuff zeros at the end to align to nearest boundary
		stuffing_bits = 32 - ((payload_length + 8) % 32);
		DEBUG_TRACE("ArticTransceiver::send_packet: adding %u stuffing bits for alignment", stuffing_bits);
	}

	uint8_t length_encoded[] = { 0x0, 0x3, 0x5, 0x6, 0x9, 0xA, 0xC, 0xF };
	unsigned int length_idx = (payload_length - 8) / 32;
	unsigned int length_enc = length_encoded[length_idx];

	unsigned int num_tail_bits = 0; // A2 mode
	if (mode == ArgosMode::ARGOS_3) {
		uint8_t tail_bits[] = { 7, 8, 9, 7, 8, 9, 7, 8 };
		num_tail_bits = tail_bits[length_idx];
	}
	unsigned int op_offset = 24;  // Keep first 24 bits spare for length field
	unsigned int ip_offset = 0;

	// Transmission is:
	// MSG_LEN (4)
	// ARGOSID (28)
	// USER_PAYLOAD (n*32 - 8)
	// STUFFING_BITS
	// TAIL_BITS (7,8,9)
	total_bits = 28 + 4 + payload_length + stuffing_bits + num_tail_bits;

	// Assign the buffer to include 24-bit length indicator and rounded-up to 3 bytes for XMEM alignment
	packet.assign(((((total_bits + 24) + 23) / 24) * 24) / 8, 0);

	// Set header
	PACK_BITS(length_enc, packet, op_offset, 4);
	PACK_BITS(argos_id>>8, packet, op_offset, 20);
	PACK_BITS(argos_id, packet, op_offset, 8);

	// Append user payload
	unsigned int payload_bits_remaining = payload_length;
	uint8_t byte;
	while (payload_bits_remaining) {
		unsigned int bits = std::min(8U, payload_bits_remaining);
		payload_bits_remaining -= bits;
		EXTRACT_BITS(byte, user_payload, ip_offset, bits);
		PACK_BITS(byte, packet, op_offset, bits);
	}

	// Add any stuffing bits
	payload_bits_remaining = stuffing_bits;
	while (payload_bits_remaining) {
		unsigned int bits = std::min(8U, payload_bits_remaining);
		payload_bits_remaining -= bits;
		PACK_BITS(0, packet, op_offset, bits);
	}

	// Add tail bits
	PACK_BITS(0, packet, op_offset, num_tail_bits);

	// Setup TX mode
	uint8_t cmd = (mode == ArgosMode::ARGOS_3) ? ARTIC_CMD_SET_PTT_A3_TX_MODE : ARTIC_CMD_SET_PTT_A2_TX_MODE;

	// Send asynchronous command to enable TX mode and start packet transfer
	send_command_check_clean_async(cmd, INTERRUPT_1, MCU_COMMAND_ACCEPTED, SAT_ARTIC_DELAY_INTERRUPT_MS,
		[this, total_bits, num_tail_bits, packet]() {

		// Set 24-bit total length field (required at the head of the packet data)
		uint8_t *buffer = (uint8_t *)packet.data();
		buffer[0] = total_bits >> 16;
		buffer[1] = total_bits >> 8;
		buffer[2] = total_bits;
	    burst_access(XMEM, TX_PAYLOAD_ADDRESS, (const uint8_t *)buffer, nullptr, packet.length(), false);

		print_status();

		DEBUG_TRACE("ArticTransceiver::send_packet: data[%u]=%s tail=%u",
				total_bits, Binascii::hexlify(packet).c_str(),
				num_tail_bits);

		// Send to ARTIC the command for sending only one packet and wait for the response TX_FINISHED
		send_command_check_clean_async(ARTIC_CMD_START_TX_1M_SLEEP, INTERRUPT_1, TX_FINISHED, SAT_ARTIC_TIMEOUT_SEND_TX_MS,
			[this]() {
				m_notification_callback(ArgosAsyncEvent::TX_DONE);
		});
	});
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
#ifndef BOARD_LINKIT
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
