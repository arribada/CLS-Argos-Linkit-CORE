#ifndef __ARTIC_HPP_
#define __ARTIC_HPP_

#include <stdint.h>

#include "argos_scheduler.hpp"
#include "nrf_spim.hpp"

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
	NrfSPIM *m_nrf_spim;
	void configure_burst(mem_id_t mode, bool read, uint32_t start_address);
	void burst_access(mem_id_t mode, uint32_t start_address, const uint8_t *tx_data, uint8_t *rx_data, size_t size, bool read);
	void send_burst(const uint8_t *tx_data, uint8_t *rx_data, size_t size, uint8_t length_transfer, bool read);
	void send_command(uint8_t command);
	void get_status_register(uint32_t *status);
	void print_status(void);
	void set_tcxo_warmup_time(uint8_t time_s);
	void program_firmware();
	void send_fw_files();
	void hardware_init();
	void send_command_check_clean(uint8_t command, uint8_t interrupt_number, uint8_t status_flag_number, bool value, uint32_t interrupt_timeout);
	void wait_interrupt(uint32_t timeout, uint8_t interrupt_num);
	void clear_interrupt(uint8_t interrupt_num);
	void check_crc(firmware_header_t *firmware_header);
	void send_artic_command(artic_cmd_t cmd, uint32_t *response);
	void spi_read(mem_id_t mode, uint32_t start_address, uint8_t *buffer_read, size_t size);
	inline uint8_t convert_mem_sel(mem_id_t mode);
	inline mem_id_t convert_mode(uint8_t mem_sel);

public:
	ArticTransceiver();
	void power_off() override;
	void power_on() override;
	void send_packet(ArgosPacket const& packet, unsigned int total_bits, const ArgosMode mode) override;
	void set_frequency(const double freq) override;
	void set_tx_power(const BaseArgosPower power) override;
};

#endif // __ARTIC_HPP_
