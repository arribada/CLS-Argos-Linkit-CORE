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
#include "timer.hpp"
#include "binascii.hpp"
#include "crc8.hpp"
#include "crc16.hpp"
#include "rtc.hpp"

#include "nrf_delay.h"
#include "nrf_gpio.h"

#define SAT_ARTIC_DELAY_TICK_INTERRUPT_MS 10
#define INVALID_MEM_SELECTION (0xFF)

#ifndef DEFAULT_TCXO_WARMUP_TIME_SECONDS
#define DEFAULT_TCXO_WARMUP_TIME_SECONDS 5
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
extern Timer *system_timer;
extern RTC *rtc;


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


bool ArticTransceiver::check_crc(firmware_header_t *firmware_header)
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
        return false;
    }

    // Check CRC Value XMEM
    crc = 0;
    reverse_memcpy((uint8_t *)&crc, &crc_buffer[3], SIZE_SPI_REG_XMEM_YMEM_IOMEM);
    if (firmware_header->XMEM_CRC != crc)
    {
        DEBUG_ERROR("ArticTransceiver::check_crc: XMEM CRC 0x%08lX DOESN'T MATCH EXPECTED 0x%08lX", crc, firmware_header->XMEM_CRC);
        return false;
    }

    // Check CRC Value YMEM
    crc = 0;
    reverse_memcpy((uint8_t *)&crc, &crc_buffer[6], SIZE_SPI_REG_XMEM_YMEM_IOMEM);
    if (firmware_header->YMEM_CRC != crc)
    {
        DEBUG_ERROR("ArticTransceiver::check_crc: YMEM CRC 0x%08lX DOESN'T MATCH EXPECTED 0x%08lX", crc, firmware_header->YMEM_CRC);
        return false;
    }

    DEBUG_TRACE("ArticTransceiver::check_crc: CRC values all match");
    return true;
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

void ArticTransceiver::get_and_print_status(void)
{
    uint32_t status;
    get_status_register(&status);

    for (uint32_t i = 0; i < TOTAL_NUMBER_STATUS_FLAG; ++i)
    {
        if (status & (1 << i)) {
            DEBUG_TRACE("ArticTransceiver::status_bit=%s", (char *)status_string[i]);
        }
    }
}

void ArticTransceiver::print_status(uint32_t status)
{
    for (uint32_t i = 0; i < TOTAL_NUMBER_STATUS_FLAG; ++i)
    {
        if (status & (1 << i)) {
            DEBUG_TRACE("ArticTransceiver::status_bit=%s", (char *)status_string[i]);
        }
    }
}

void ArticTransceiver::clear_interrupt(uint8_t interrupt_num)
{
    switch (interrupt_num)
    {
        case INTERRUPT_1:
            send_command(ARTIC_CMD_CLEAR_INT1);
            PMU::delay_ms(SAT_ARTIC_DELAY_TICK_INTERRUPT_MS); // Wait for IRQ to clear
            break;
        case INTERRUPT_2:
            send_command(ARTIC_CMD_CLEAR_INT2);
            PMU::delay_ms(SAT_ARTIC_DELAY_TICK_INTERRUPT_MS); // Wait for IRQ to clear
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

void ArticTransceiver::set_tcxo_warmup(uint32_t time_s)
{
    uint8_t write_buffer[3];
    write_buffer[0] = (time_s) >> 16;
    write_buffer[1] = (time_s) >> 8;
    write_buffer[2] = (time_s);

    burst_access(XMEM, TCXO_WARMUP_TIME_ADDRESS, write_buffer, NULL, sizeof(write_buffer), false);
}

void ArticTransceiver::set_tcxo_control(bool state)
{
    uint8_t write_buffer[3];
    write_buffer[0] = 0;
    write_buffer[1] = 0;
    write_buffer[2] = state;

    burst_access(XMEM, TCXO_CONTROL_ADDRESS, write_buffer, NULL, sizeof(write_buffer), false);
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

    // The state machine will safely transition to stopped as soon
    // as all pending activities have completed
    if (!ARTIC_STATE_EQUAL(stopped)) {
    	m_stopping = true;
    }
}

void ArticTransceiver::power_off_immediate()
{
    DEBUG_TRACE("ArticTransceiver::power_off_immediate");

    // We force a power off by cancelling the state machine and
    // forcing a stopped state
    if (!ARTIC_STATE_EQUAL(stopped)) {
    	system_scheduler->cancel_task(m_task);
    	ARTIC_STATE_CHANGE(idle, stopped);
    }
}

void ArticTransceiver::add_rx_packet_filter(const uint32_t address) {

	// Read current filter entry count
	uint32_t tmp = 0, lut_size = 0, value = 0;
    burst_access(XMEM, RX_FILTERING_CONFIG + 3, nullptr, (uint8_t *)&tmp, 3, true);
    reverse_memcpy((uint8_t *)&lut_size, (const uint8_t *)&tmp, 3);

    // Scan existing filter table to make sure the entry does not already exist
    for (unsigned int i = 0; i < lut_size; i++) {
    	uint32_t existing_addr = 0;

    	// Get 24 LSBs of address
        burst_access(XMEM, RX_FILTERING_CONFIG + 4 + (2*i), nullptr, (uint8_t *)&tmp, 3, true);
        reverse_memcpy((uint8_t *)&existing_addr, (const uint8_t *)&tmp, 3);

        // Get 4 MSBs of address
        burst_access(XMEM, RX_FILTERING_CONFIG + 5 + (2*i), nullptr, (uint8_t *)&tmp, 3, true);
        reverse_memcpy((uint8_t *)&value, (const uint8_t *)&tmp, 3);
        existing_addr |= (value & 0xF) << 24;

        // Don't add a new filter if it already exists in the table
        if (existing_addr == address) {
        	DEBUG_TRACE("ArticTransceiver::add_rx_packet_filter: address=%08x already in table", address);
        	return;
        }
    }

	DEBUG_TRACE("ArticTransceiver::add_rx_packet_filter: new address=%08x added", address);

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
	m_pa_driver = nullptr;
    m_nrf_spim = nullptr;
    m_state = ArticTransceiverState::stopped;
    m_tcxo_warmup_time = DEFAULT_TCXO_WARMUP_TIME_SECONDS;
	GPIOPins::clear(SAT_RESET);
	GPIOPins::clear(SAT_PWR_EN);
}

void ArticTransceiver::power_on(
		const unsigned int argos_id,
		std::function<void(ArgosAsyncEvent)> notification_callback)
{
    DEBUG_TRACE("ArticTransceiver::power_on");

    // Device is already powered
    if (m_state != ArticTransceiverState::stopped) {
    	m_stopping = false;  // Clear any pending stopping flag
    	DEBUG_TRACE("ArticTransceiver::power_on: not disabled: state=%u", (unsigned int)m_state);
    	return;
    }

    // Set top-level state variables
    m_argos_id = argos_id;
    m_notification_callback = notification_callback;
    m_state = ArticTransceiverState::starting; // Force a reset when the task starts
    m_stopping = false;

    // Reset state variables which can be used prior to being fully programmed
    m_ack_buffer.clear();
    m_packet_buffer.clear();
    m_rx_packet.clear();

    // This will run the state machine perpetually until a power_off is called
    state_machine();
}

void ArticTransceiver::state_machine() {

	// Assume no delay between state machine invocations
	m_next_delay = 0;

	switch (m_state) {
	case ArticTransceiverState::starting:
		ARTIC_STATE_CALL(starting);
		break;
	case ArticTransceiverState::powering_on:
		ARTIC_STATE_CALL(powering_on);
		break;
	case ArticTransceiverState::reset_assert:
		ARTIC_STATE_CALL(reset_assert);
		break;
	case ArticTransceiverState::reset_deassert:
		ARTIC_STATE_CALL(reset_deassert);
		break;
	case ArticTransceiverState::dsp_reset:
		ARTIC_STATE_CALL(dsp_reset);
		break;
	case ArticTransceiverState::send_firmware_image:
		ARTIC_STATE_CALL(send_firmware_image);
		break;
	case ArticTransceiverState::wait_firmware_ready:
		ARTIC_STATE_CALL(wait_firmware_ready);
		break;
	case ArticTransceiverState::check_firmware_crc:
		ARTIC_STATE_CALL(check_firmware_crc);
		break;
	case ArticTransceiverState::idle_pending:
		ARTIC_STATE_CALL(idle_pending);
		break;
	case ArticTransceiverState::idle:
		ARTIC_STATE_CALL(idle);
		break;
	case ArticTransceiverState::receive_pending:
		ARTIC_STATE_CALL(receive_pending);
		break;
	case ArticTransceiverState::receiving:
		ARTIC_STATE_CALL(receiving);
		break;
	case ArticTransceiverState::transmit_pending:
		ARTIC_STATE_CALL(transmit_pending);
		break;
	case ArticTransceiverState::transmitting:
		ARTIC_STATE_CALL(transmitting);
		break;
	case ArticTransceiverState::error:
		ARTIC_STATE_CALL(error);
		break;
	case ArticTransceiverState::stopped:
		ARTIC_STATE_CALL(stopped);
	default:
		break;
	}

	if (!ARTIC_STATE_EQUAL(stopped)) {
		// Invoke ourselves again if we are not stopped
		//DEBUG_TRACE("ArticTransceiver::state_machine: reschedule in %u ms", m_next_delay);
		m_task = system_scheduler->post_task_prio([this]() {
			state_machine();
		}, "ArgosTransceiverStateMachine", Scheduler::DEFAULT_PRIORITY, m_next_delay);
	}
}

void ArticTransceiver::state_starting_enter() {
}

void ArticTransceiver::state_starting_exit() {
}

void ArticTransceiver::state_starting() {
    m_rx_packet_bits = 0;
    m_rx_total_time = 0;
    m_is_first_tx = true;
	m_nrf_spim = new NrfSPIM(SPI_SATELLITE);
	m_pa_driver = new PADriver();
	m_rx_pending = false;
	ARTIC_STATE_CHANGE(starting, powering_on);
}

void ArticTransceiver::state_error_enter() {
	// Dump device status
	get_and_print_status();

	// The calling code's callback should call "power_off" and
	// do any necessary cleanup that it needs to otherwise
	// we will keep invoking the callback!
	m_notification_callback(ArgosAsyncEvent::ERROR);
}

void ArticTransceiver::state_error_exit() {
}

void ArticTransceiver::state_error() {
	if (m_stopping) {
		ARTIC_STATE_CHANGE(error, stopped);
	}
}

void ArticTransceiver::state_stopped_enter() {
	// Cleanup the SPIM instance
	delete m_nrf_spim;
    m_nrf_spim = nullptr; // Invalidate this pointer so if we call this function again it doesn't call delete on an invalid pointer

    // Cleanup PA driver
    delete m_pa_driver;
    m_pa_driver = nullptr;

	// FIXME: should this be moved into the NrfSPIM driver?
	nrf_gpio_cfg_output(BSP::SPI_Inits[SPI_SATELLITE].config.ss_pin);
	nrf_gpio_pin_clear(BSP::SPI_Inits[SPI_SATELLITE].config.ss_pin);
    nrf_gpio_cfg_input(BSP::SPI_Inits[SPI_SATELLITE].config.mosi_pin, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(BSP::SPI_Inits[SPI_SATELLITE].config.miso_pin, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(BSP::SPI_Inits[SPI_SATELLITE].config.sck_pin, NRF_GPIO_PIN_PULLDOWN);

    // Power down the device
	GPIOPins::clear(SAT_RESET);
	GPIOPins::clear(SAT_PWR_EN);

	m_notification_callback(ArgosAsyncEvent::OFF);
}

void ArticTransceiver::state_stopped_exit() {
}

void ArticTransceiver::state_stopped() {
}

void ArticTransceiver::state_powering_on_enter() {
}

void ArticTransceiver::state_powering_on_exit() {
	m_next_delay = SAT_ARTIC_DELAY_POWER_ON_MS;
}

void ArticTransceiver::state_powering_on() {
    GPIOPins::set(SAT_PWR_EN);
    GPIOPins::set(SAT_RESET);
    ARTIC_STATE_CHANGE(powering_on, reset_assert);
}

void ArticTransceiver::state_reset_assert_enter() {
}

void ArticTransceiver::state_reset_assert_exit() {
	m_next_delay = SAT_ARTIC_DELAY_RESET_MS;
}

void ArticTransceiver::state_reset_assert() {
    GPIOPins::clear(SAT_RESET);
    ARTIC_STATE_CHANGE(reset_assert, reset_deassert);
}

void ArticTransceiver::state_reset_deassert_enter() {
	m_next_delay = SAT_ARTIC_DELAY_RESET_MS;
}

void ArticTransceiver::state_reset_deassert_exit() {
	m_next_delay = SAT_ARTIC_DELAY_RESET_MS;
}

void ArticTransceiver::state_reset_deassert() {
    GPIOPins::set(SAT_RESET);
    ARTIC_STATE_CHANGE(reset_deassert, dsp_reset);
}

void ArticTransceiver::state_dsp_reset_enter() {
	m_polling_counter = 3;
}

void ArticTransceiver::state_dsp_reset_exit() {
}

void ArticTransceiver::state_dsp_reset() {
	uint32_t artic_response = 0;

	// Get DSP status
	send_artic_command(DSP_STATUS, &artic_response);
	DEBUG_TRACE("DSP_STATUS=%02x", artic_response);

	// Check response
	if (artic_response == 0x55) {
		ARTIC_STATE_CHANGE(dsp_reset, send_firmware_image);
	} else {
		// Check retries
		if (--m_polling_counter == 0) {
			// Failed to reset DSP
			DEBUG_ERROR("ArticTransceiver::state_machine: DSP reset failed");
			ARTIC_STATE_CHANGE(dsp_reset, error);
		} else
			m_next_delay = SAT_ARTIC_DELAY_BOOT_MS;
	}
}

void ArticTransceiver::state_send_firmware_image_enter() {
	// Reset firmware programming state
    m_mem_sel = 0;
    m_bytes_total_read = 0;

    // Reset firmware file read position to start before reading header
    m_firmware_file.seek(0);
    m_firmware_file.read((unsigned char *)&m_firmware_header, sizeof(m_firmware_header));
}

void ArticTransceiver::state_send_firmware_image_exit() {
}

void ArticTransceiver::state_send_firmware_image() {
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
			return;
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
	if (m_mem_sel >= NUM_FIRMWARE_FILES_ARTIC) {
		ARTIC_STATE_CHANGE(send_firmware_image, wait_firmware_ready);
	}
}

void ArticTransceiver::state_wait_firmware_ready_enter() {
	m_polling_counter = 100;
	send_artic_command(DSP_CONFIG, nullptr);
}

void ArticTransceiver::state_wait_firmware_ready_exit() {
}

void ArticTransceiver::state_wait_firmware_ready() {

	// Check response
	if (is_firmware_ready()) {
		ARTIC_STATE_CHANGE(wait_firmware_ready, check_firmware_crc);
	} else {
		// Check retries
		if (--m_polling_counter == 0) {
			// Failed to start firmware
			DEBUG_ERROR("ArticTransceiver::state_machine: firmware did not start");
			ARTIC_STATE_CHANGE(wait_firmware_ready, error);
		} else
			m_next_delay = SAT_ARTIC_DELAY_TICK_INTERRUPT_MS;
	}
}

void ArticTransceiver::state_check_firmware_crc_enter() {
}

void ArticTransceiver::state_check_firmware_crc_exit() {
}

void ArticTransceiver::state_check_firmware_crc() {
	if (check_crc(&m_firmware_header))
		ARTIC_STATE_CHANGE(check_firmware_crc, idle);
	else {
		DEBUG_ERROR("ArticTransceiver::state_check_crc: CRC failure");
		ARTIC_STATE_CHANGE(check_firmware_crc, error);
	}
}

void ArticTransceiver::state_idle_pending_enter() {
	m_polling_counter = 100;
	send_command(ARTIC_CMD_SLEEP);
}

void ArticTransceiver::state_idle_pending_exit() {
}

void ArticTransceiver::state_idle_pending() {

	if (is_idle_state()) {
		// Clear the interrupt
        clear_interrupt(INTERRUPT_1);
        ARTIC_STATE_CHANGE(idle_pending, idle);
	} else {
		// Check retries
		if (--m_polling_counter == 0) {
			// Failed to go IDLE
			DEBUG_ERROR("ArticTransceiver::state_machine: failed to enter IDLE state");
	        ARTIC_STATE_CHANGE(idle_pending, error);
		} else
			m_next_delay = SAT_ARTIC_DELAY_TICK_INTERRUPT_MS;
	}
}

void ArticTransceiver::state_idle_enter() {
	// Check if we are already IDLE or not
	if (is_idle()) {
		return;
	}

	ARTIC_STATE_CHANGE(idle, idle_pending);
}

void ArticTransceiver::state_idle_exit() {
}

void ArticTransceiver::state_idle() {
	// Check for any new commands
	if (m_packet_buffer.length()) {
		m_tx_event = ArgosAsyncEvent::TX_DONE;
		m_tx_buffer = m_packet_buffer;
		m_packet_buffer.clear();
		ARTIC_STATE_CHANGE(idle, transmit_pending);
	} else if (m_ack_buffer.length()) {
		m_tx_event = ArgosAsyncEvent::ACK_DONE;
		m_tx_buffer = m_ack_buffer;
		m_ack_buffer.clear();
		ARTIC_STATE_CHANGE(idle, transmit_pending);
	} else if (m_rx_pending) {
		ARTIC_STATE_CHANGE(idle, receive_pending);
	} else if (m_stopping) {
		ARTIC_STATE_CHANGE(idle, stopped);
	}
}

void ArticTransceiver::state_receive_pending_enter() {
	m_polling_counter = 100;
	initiate_rx();
}

void ArticTransceiver::state_receive_pending_exit() {
}

void ArticTransceiver::state_receive_pending() {

	if (is_command_accepted()) {

		// Clear the interrupt
        clear_interrupt(INTERRUPT_1);

        ARTIC_STATE_CHANGE(receive_pending, receiving);
	} else {
		// Check retries
		if (--m_polling_counter == 0) {
			DEBUG_ERROR("ArticTransceiver::state_machine: failed accept RX mode command");
			ARTIC_STATE_CHANGE(receive_pending, error);
		} else
			m_next_delay = SAT_ARTIC_DELAY_TICK_INTERRUPT_MS;
	}
}

void ArticTransceiver::state_receiving_enter() {
	send_command(ARTIC_CMD_START_RX_CONT);
	m_rx_timer_start = system_timer->get_counter();
}

void ArticTransceiver::state_receiving_exit() {
	m_rx_total_time += system_timer->get_counter() - m_rx_timer_start;
}

void ArticTransceiver::state_receiving() {

	// Check for any other commands that take priority over RX
	if (m_packet_buffer.length() || m_ack_buffer.length()) {
		ARTIC_STATE_CHANGE(receiving, idle); // Transition to IDLE for commands to be run
		return;
	} else if (m_stopping) {
		ARTIC_STATE_CHANGE(idle, stopped);
		return;
	}

	// Check for an RX timeout condition
	if (rtc->gettime() >= m_rx_stop_time) {
		DEBUG_TRACE("ArticTransceiver::state_receiving: timeout detected");

		// Clear any pending interrupts
	    clear_interrupt(INTERRUPT_1);

		// Initiate IDLE state
		ARTIC_STATE_CHANGE(receiving, idle);

		// Notify user of timeout condition
		m_notification_callback(ArgosAsyncEvent::RX_TIMEOUT);
		return;
	}

    // Check for valid RX message
    if (is_rx_available()) {
    	// Read packet data locally
    	bool valid_packet_available = buffer_rx_packet();

        // Clear the interrupt
        clear_interrupt(INTERRUPT_1);

        // Notify if new RX packet has been stored
        if (valid_packet_available)
        	m_notification_callback(ArgosAsyncEvent::RX_PACKET);
    }
}

void ArticTransceiver::state_transmit_pending_enter() {
	m_polling_counter = 100;
	initiate_tx();
}

void ArticTransceiver::state_transmit_pending_exit() {
}

void ArticTransceiver::state_transmit_pending() {

	if (is_command_accepted()) {

		// Clear the interrupt
        clear_interrupt(INTERRUPT_1);
        m_notification_callback(ArgosAsyncEvent::TX_STARTED);
        ARTIC_STATE_CHANGE(transmit_pending, transmitting);
	} else {
		// Check retries
		if (--m_polling_counter == 0) {
			DEBUG_ERROR("ArticTransceiver::state_machine: failed accept PTT TX command");
	        ARTIC_STATE_CHANGE(transmit_pending, error);
		} else
			m_next_delay = SAT_ARTIC_DELAY_TICK_INTERRUPT_MS;
	}
}

void ArticTransceiver::state_transmitting_enter() {
	m_polling_counter = 500;
	send_command(ARTIC_CMD_START_TX_1M_SLEEP);
}

void ArticTransceiver::state_transmitting_exit() {
	m_is_first_tx = false;
	m_pa_driver->set_output_power(0);
}

void ArticTransceiver::state_transmitting() {

	if (is_tx_finished()) {
		// Clear the interrupt
        clear_interrupt(INTERRUPT_1);
        m_notification_callback(m_tx_event);
        ARTIC_STATE_CHANGE(transmitting, idle);
	} else {
		// Check retries
		if (--m_polling_counter == 0) {
			DEBUG_ERROR("ArticTransceiver::state_machine: failed to complete TX");
			ARTIC_STATE_CHANGE(transmitting, error);
		} else
			m_next_delay = SAT_ARTIC_DELAY_TICK_INTERRUPT_MS;
	}
}

void ArticTransceiver::initiate_rx() {

	// Clear pending flag
	m_rx_pending = false;

	// Add new entries for filtering on new packet types added since the firmware
	// image was released
	add_rx_packet_filter(0x000005F); // Constellation Satellite Status - format B
	add_rx_packet_filter(0x00000D4); // Satellite Orbit Parameters – format B for ANGELS
	add_rx_packet_filter(m_argos_id);

	// Initiate RX mode command
	send_command((m_rx_mode == ArgosMode::ARGOS_3) ? ARTIC_CMD_SET_ARGOS_3_RX_MODE : ARTIC_CMD_SET_ARGOS_4_RX_MODE);
}

void ArticTransceiver::initiate_tx() {

	// Set TCXO frequency
	unsigned int fractional_part;
	fractional_part = (unsigned int)std::round((((4.0 * m_tx_freq * 1E6) / 26E6) - 61.0) * 4194304.0);

    uint8_t write_buffer[3];
    write_buffer[0] = (fractional_part) >> 16;
    write_buffer[1] = (fractional_part) >> 8;
    write_buffer[2] = (fractional_part);

	// Only supports Argos 2 and 3 band
	burst_access(XMEM, TX_FREQUENCY_ARGOS_2_3, write_buffer, nullptr, sizeof(write_buffer), false);

	// Program TX buffer
	burst_access(XMEM, TX_PAYLOAD_ADDRESS, (const uint8_t *)&m_tx_buffer[0], nullptr, m_tx_buffer.length(), false);

	// Set TCXO warm up
	set_tcxo_warmup(m_is_first_tx ? m_tcxo_warmup_time : 0);
	set_tcxo_control(1);  // Keep TCXO on after command

	// Check which mode is required
	uint8_t cmd = (m_tx_mode == ArgosMode::ARGOS_3) ? ARTIC_CMD_SET_PTT_A3_TX_MODE : ARTIC_CMD_SET_PTT_A2_TX_MODE;

	// Send command to enable TX mode and start packet transfer
	send_command(cmd);
}

bool ArticTransceiver::is_firmware_ready() {
	uint32_t status = 0, required = (1<<IDLE)|(1<<RX_CALIBRATION_FINISHED);
    get_status_register(&status);
    //print_status(status);
    return ((status & required) == required);
}

bool ArticTransceiver::is_tx_finished() {
	uint32_t status = 0, required = (1<<TX_FINISHED);
    get_status_register(&status);
    print_status(status);
    return ((status & required) == required);
}

bool ArticTransceiver::is_rx_in_progress() {
	uint32_t status = 0, required = (1<<RX_IN_PROGRESS);
    get_status_register(&status);
    //print_status(status);
    return ((status & required) == required);
}

bool ArticTransceiver::is_tx_in_progress() {
	uint32_t status = 0, required = (1<<TX_IN_PROGRESS);
    get_status_register(&status);
    //print_status(status);
    return ((status & required) == required);
}

bool ArticTransceiver::is_rx_available() {
	uint32_t status = 0, required = (1<<RX_VALID_MESSAGE);
    get_status_register(&status);
    //print_status(status);
    return ((status & required) == required);
}

bool ArticTransceiver::is_command_accepted() {
	uint32_t status = 0, required = (1<<MCU_COMMAND_ACCEPTED);
    get_status_register(&status);
    //print_status(status);
    return ((status & required) == required);
}

bool ArticTransceiver::is_idle() {
	uint32_t status = 0, required = (1<<IDLE);
    get_status_register(&status);
    //print_status(status);
    return ((status & required) == required);
}

bool ArticTransceiver::is_idle_state() {
	uint32_t status = 0, required = (1<<IDLE_STATE);
    get_status_register(&status);
    //print_status(status);
    return ((status & required) == required);
}

void ArticTransceiver::read_packet(ArgosPacket& packet, unsigned int& size) {
	packet = m_rx_packet;
	size = m_rx_packet_bits;
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

void ArticTransceiver::set_rx_mode(const ArgosMode mode, const std::time_t stop_time) {

	DEBUG_TRACE("ArticTransceiver::set_rx_mode(%u,%llu)", (unsigned int)mode, stop_time);
	m_rx_stop_time = stop_time;
	m_rx_mode = mode;
	m_rx_pending = true;
}

uint64_t ArticTransceiver::get_rx_time_on() {
	uint64_t t = m_rx_total_time;
	m_rx_total_time = 0;
	return t;
}

void ArticTransceiver::send_packet(ArgosPacket const& user_payload, unsigned int payload_length, const ArgosMode mode)
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
	unsigned int length_idx = (stuffing_bits + payload_length - 8) / 32;
	unsigned int length_enc = length_encoded[length_idx];

	unsigned int num_tail_bits = 0; // A2 mode
	if (mode == ArgosMode::ARGOS_3) {
		uint8_t tail_bits[] = { 7, 8, 9, 7, 8, 9, 7, 8 };
		num_tail_bits = tail_bits[length_idx];
	}
	unsigned int op_offset = 0;
	unsigned int ip_offset = 0;

	// Transmission is:
	// MSG_LEN (4)
	// ARGOSID (28)
	// USER_PAYLOAD (n*32 - 8)
	// STUFFING_BITS
	// TAIL_BITS (7,8,9)
	total_bits = 4 + 28 + payload_length + stuffing_bits + num_tail_bits;

	// Assign the buffer to include 24-bit length indicator and rounded-up to 3 bytes for XMEM alignment
	packet.assign(((((total_bits + 24) + 23) / 24) * 24) / 8, 0);

	// Set total length
	PACK_BITS(total_bits, packet, op_offset, 24);

	// Set header
	PACK_BITS(length_enc, packet, op_offset, 4);
	PACK_BITS(m_argos_id, packet, op_offset, 28);

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

	// Setup TX operation
	DEBUG_TRACE("ArticTransceiver::send_packet: data[%u]=%s tail=%u",
			total_bits, Binascii::hexlify(packet).c_str(),
			num_tail_bits);

	// Setup TX mode
	m_tx_mode = mode;
	m_packet_buffer = packet;
}


void ArticTransceiver::send_ack(const unsigned int a_dcs, const unsigned int dl_msg_id, const unsigned int exec_report, const ArgosMode mode)
{
	ArgosPacket packet, crc_packet;
	unsigned int stuffing_bits = 0;
	unsigned int payload_length = 96;
	unsigned int length_enc = 0x5;
	unsigned int service_id = 0x00EBA;
	unsigned int total_bits;

	DEBUG_TRACE("ArticTransceiver::send_ack");

	unsigned int num_tail_bits = 0; // A2 mode
	if (mode == ArgosMode::ARGOS_3)
		num_tail_bits = 7;

	// Transmission is:
	// LENGTH (24)
	// MSG_LEN (4)
	// SERVICEID (20)  + *
	// FCS (16)        |
	// ADCS (4)        | *
	// PMTID (28)      | *
	// DLMSGID (16)    | *
	// EXECRPT(4)      | *
	// DATA (28)       + *
	// STUFFING (0)
	// TAIL_BITS (7)
	total_bits = 24 + 4 + 20 + payload_length + stuffing_bits + num_tail_bits;

	unsigned int crc_offset = 0;
	unsigned int op_offset = 0;

	// Assign a temporary buffer for computing the FCS over the required* fields
	crc_packet.assign(13, 0); // 13 bytes, 100 bits
	PACK_BITS(service_id, crc_packet, crc_offset, 20);
	PACK_BITS(a_dcs, crc_packet, crc_offset, 4);
	PACK_BITS(m_argos_id, crc_packet, crc_offset, 28);
	PACK_BITS(dl_msg_id, crc_packet, crc_offset, 16);
	PACK_BITS(exec_report, crc_packet, crc_offset, 4);
	PACK_BITS(0, crc_packet, crc_offset, 28);

	// Compute FCS
	uint16_t fcs = CRC16::checksum(crc_packet, 100);

	// Assign the buffer rounded-up to 3 bytes for XMEM alignment
	packet.assign((((total_bits + 23) / 24) * 24) / 8, 0);

	PACK_BITS(total_bits, packet, op_offset, 24);
	PACK_BITS(length_enc, packet, op_offset, 4);
	PACK_BITS(service_id, packet, op_offset, 20);
	PACK_BITS(fcs, packet, op_offset, 16);
	PACK_BITS(a_dcs, packet, op_offset, 4);
	PACK_BITS(m_argos_id, packet, op_offset, 28);
	PACK_BITS(dl_msg_id, packet, op_offset, 16);
	PACK_BITS(exec_report, packet, op_offset, 4);
	PACK_BITS(0, packet, op_offset, 28);
	PACK_BITS(0, packet, op_offset, stuffing_bits);
	PACK_BITS(0, packet, op_offset, num_tail_bits);

	// Setup ACK mode
	m_ack_mode = mode;
	m_ack_buffer = packet;
}

void ArticTransceiver::set_frequency(const double freq) {
	m_tx_freq = freq;
}

void ArticTransceiver::set_tcxo_warmup_time(const unsigned int time_s) {
	m_tcxo_warmup_time = time_s;
}

void ArticTransceiver::set_tx_power(const BaseArgosPower power) {
#ifndef BOARD_LINKIT
	(void)power;
#else
	switch (power) {
	case BaseArgosPower::POWER_40_MW:
		m_pa_driver->set_output_power(40);
		break;
	case BaseArgosPower::POWER_500_MW:
		m_pa_driver->set_output_power(500);
		break;
	case BaseArgosPower::POWER_200_MW:
		m_pa_driver->set_output_power(200);
		break;
	case BaseArgosPower::POWER_3_MW:
		m_pa_driver->set_output_power(3);
		break;
	case BaseArgosPower::POWER_5_MW:
		m_pa_driver->set_output_power(5);
		break;
	case BaseArgosPower::POWER_50_MW:
		m_pa_driver->set_output_power(50);
		break;
	case BaseArgosPower::POWER_350_MW:
		m_pa_driver->set_output_power(350);
		break;
	case BaseArgosPower::POWER_750_MW:
		m_pa_driver->set_output_power(750);
		break;
	case BaseArgosPower::POWER_1000_MW:
		m_pa_driver->set_output_power(1000);
		break;
	case BaseArgosPower::POWER_1500_MW:
		m_pa_driver->set_output_power(1500);
		break;
	default:
		break;

	}
#endif
}
