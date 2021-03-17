#ifndef __SERVICE_SCHEDULER_HPP_
#define __SERVICE_SCHEDULER_HPP_

#include <functional>

enum class ServiceEvent {
	SENSOR_LOG_UPDATED,
	ARGOS_TX_START,
	ARGOS_TX_END
};

class ServiceScheduler {
public:
	virtual ~ServiceScheduler() {}
	virtual void start(std::function<void(ServiceEvent)> data_notification_callback = nullptr) = 0;
	virtual void stop() = 0;
	virtual void notify_saltwater_switch_state(bool state) = 0;
	virtual void notify_sensor_log_update() = 0;
};

#endif // __SERVICE_SCHEDULER_HPP_
