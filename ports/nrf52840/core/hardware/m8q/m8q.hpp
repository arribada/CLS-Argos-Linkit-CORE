#pragma once

#include "gps_scheduler.hpp"

class M8QReceiver : public GPSScheduler {
private:
	// These methods are specific to the chipset and should be implemented by device-specific subclass
	void power_off() override;
	void power_on(std::function<void(GNSSData data)> data_notification_callback) override;
};
