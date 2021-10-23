#pragma once

#include <stdint.h>

#include "argos_scheduler.hpp"
#include "nrf_spim.hpp"
#include "artic_firmware.hpp"

#define MAX_BURST  (2048)


#define ARTIC_STATE_CHANGE(x, y)                     \
	do {                                             \
		DEBUG_TRACE("ARTIC_STATE_CHANGE: " #x " -> " #y ); \
		m_state = y;                                 \
		state_ ## x ##_exit();                       \
		state_ ## y ##_enter();                      \
	} while (0)

#define ARTIC_STATE_EQUAL(x) \
	(m_state == x)

#define ARTIC_STATE_CALL(x) \
	do {                    \
		DEBUG_TRACE("ARTIC_STATE: " #x); \
		state_ ## x();      \
	} while (0)


class ArticTransceiver : public ArgosScheduler {

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

	enum status_flag
	{
	    // Current Firmware state //
	    IDLE,                                   // The firmware is idle and ready to accept commands.
	    RX_IN_PROGRESS,                         // The firmware is receiving.
	    TX_IN_PROGRESS,                         // The firmware is transmitting.
	    BUSY,                                   // The firmware is changing state.

	    // Interrupt 1 flags //
	    RX_VALID_MESSAGE,                       // A message has been received.
	    RX_SATELLITE_DETECTED,                  // A satellite has been detected.
	    TX_FINISHED,                            // The transmission was completed.
	    MCU_COMMAND_ACCEPTED,                   // The configuration command has been accepted.
	    CRC_CALCULATED,                         // CRC calculation has finished.
	    IDLE_STATE,                             // Firmware returned to the idle state.
	    RX_CALIBRATION_FINISHED,                // RX offset calibration has completed.
	    RESERVED_11,
	    RESERVED_12,

	    // Interrupt 2 flags //
	    RX_TIMEOUT,                             // The specified reception time has been exceeded.
	    SATELLITE_TIMEOUT,                      // No satellite was detected within the specified time.
	    RX_BUFFER_OVERFLOW,                     // A received message is lost. No buffer space left.
	    TX_INVALID_MESSAGE,                     // Incorrect TX payload length specified.
	    MCU_COMMAND_REJECTED,                   // Incorrect command send or Firmware is not in idle.
	    MCU_COMMAND_OVERFLOW,                   // Previous command was not yet processed.
	    RESERVED_19,
	    RESERVER_20,

	    // Misc //
	    INTERNAL_ERROR,                         // An internal error has occurred.
	    dsp2mcu_int1,                           // Interrupt 1 pin status
	    dsp2mcu_int2,                           // Interrupt 2 pin status
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

private:
	enum ArticTransceiverState {
		stopped,
		starting,
		powering_on,
		reset_assert,
		reset_deassert,
		dsp_reset,
		send_firmware_image,
		wait_firmware_ready,
		check_firmware_crc,
		idle_pending,
		idle,
		receive_pending,
		receiving,
		transmit_pending,
		transmitting,
		error
	};

	// Top-level state
	ArticTransceiverState m_state;
	std::function<void(ArgosAsyncEvent)> m_notification_callback;
	NrfSPIM *m_nrf_spim;
	unsigned int m_argos_id;
	unsigned int m_polling_counter;
	unsigned int m_next_delay;
	bool m_stopping;

	// Firmware update procedure state
    mem_id_t m_mode;
    firmware_header_t m_firmware_header;
    ArticFirmwareFile m_firmware_file;
    uint32_t m_start_address;
    uint32_t m_size;
    uint32_t m_length_transfer;
    uint32_t m_last_address;
    uint8_t m_pending_buffer[MAX_BURST];
    uint32_t m_bytes_pending;
    uint8_t m_mem_sel;
    uint32_t m_bytes_total_read;

	// Argos TX state
	ArgosPacket m_tx_buffer;
	ArgosPacket m_ack_buffer;
	ArgosPacket m_packet_buffer;
	ArgosMode   m_tx_mode;
	ArgosMode   m_ack_mode;
	ArgosAsyncEvent m_tx_event;
	double      m_tx_freq;
	bool        m_is_first_tx;

	// Argos RX state
	ArgosMode    m_rx_mode;
	ArgosPacket  m_rx_packet;
	unsigned int m_rx_packet_bits;
	bool         m_rx_pending;
	uint64_t     m_rx_timer_start;
	std::time_t  m_rx_stop_time;
	uint64_t     m_rx_total_time;

	// Support functionality
	void configure_burst(mem_id_t mode, bool read, uint32_t start_address);
	void burst_access(mem_id_t mode, uint32_t start_address, const uint8_t *tx_data, uint8_t *rx_data, size_t size, bool read);
	void send_burst(const uint8_t *tx_data, uint8_t *rx_data, size_t size, uint8_t length_transfer, bool read);
	void send_command(uint8_t command);
	void get_status_register(uint32_t *status);
	void print_status(uint32_t status);
	void get_and_print_status();
	void set_tcxo_warmup_time(uint32_t time_s);
	void clear_interrupt(uint8_t interrupt_num);
	bool check_crc(firmware_header_t *firmware_header);
	void send_artic_command(artic_cmd_t cmd, uint32_t *response);
	void spi_read(mem_id_t mode, uint32_t start_address, uint8_t *buffer_read, size_t size);
	inline uint8_t convert_mem_sel(mem_id_t mode);
	inline mem_id_t convert_mode(uint8_t mem_sel);
	void print_firmware_version();
	bool buffer_rx_packet();
	void add_rx_packet_filter(const uint32_t address);
	bool is_idle();
	bool is_idle_state();
	bool is_tx_finished();
	bool is_rx_available();
	bool is_rx_in_progress();
	bool is_tx_in_progress();
	bool is_command_accepted();
	bool is_firmware_ready();
	void initiate_tx();
	void initiate_rx();

	// State machine functionality
	void state_machine();
	void state_starting();
	void state_starting_enter();
	void state_starting_exit();
	void state_stopped();
	void state_stopped_enter();
	void state_stopped_exit();
	void state_error();
	void state_error_enter();
	void state_error_exit();
	void state_powering_on();
	void state_powering_on_enter();
	void state_powering_on_exit();
	void state_reset_assert();
	void state_reset_assert_enter();
	void state_reset_assert_exit();
	void state_reset_deassert();
	void state_reset_deassert_enter();
	void state_reset_deassert_exit();
	void state_dsp_reset();
	void state_dsp_reset_enter();
	void state_dsp_reset_exit();
	void state_send_firmware_image();
	void state_send_firmware_image_enter();
	void state_send_firmware_image_exit();
	void state_wait_firmware_ready();
	void state_wait_firmware_ready_enter();
	void state_wait_firmware_ready_exit();
	void state_check_firmware_crc();
	void state_check_firmware_crc_enter();
	void state_check_firmware_crc_exit();
	void state_idle_pending();
	void state_idle_pending_enter();
	void state_idle_pending_exit();
	void state_idle();
	void state_idle_enter();
	void state_idle_exit();
	void state_transmit_pending();
	void state_transmit_pending_enter();
	void state_transmit_pending_exit();
	void state_transmitting();
	void state_transmitting_enter();
	void state_transmitting_exit();
	void state_receive_pending();
	void state_receive_pending_enter();
	void state_receive_pending_exit();
	void state_receiving();
	void state_receiving_enter();
	void state_receiving_exit();

public:
	ArticTransceiver();
	void power_off() override;
	void power_on(const unsigned int argos_id, std::function<void(ArgosAsyncEvent)> notification_callback) override;
	void send_packet(ArgosPacket const& user_payload, unsigned int payload_length, const ArgosMode mode) override;
	void send_ack(const unsigned int a_dcs, const unsigned int dl_msg_id, const unsigned int exec_report, const ArgosMode mode) override;
	void read_packet(ArgosPacket& packet, unsigned int& size) override;
	void set_rx_mode(const ArgosMode mode, const std::time_t stop_time) override;
	uint64_t get_rx_time_on() override;
	void set_frequency(const double freq) override;
	void set_tx_power(const BaseArgosPower power) override;
};
