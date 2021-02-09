#pragma once

#include "gps_scheduler.hpp"
#include "nrf_uart_m8.hpp"

class M8QReceiver : public GPSScheduler {
public:
	M8QReceiver();

private:
	// These methods are specific to the chipset and should be implemented by device-specific subclass
	void power_off() override;
	void power_on(std::function<void(GNSSData data)> data_notification_callback) override;

	NrfUARTM8 *m_nrf_uart_m8;
};
