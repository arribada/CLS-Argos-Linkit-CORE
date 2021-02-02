#ifndef __LOCATION_SCHEDULER_HPP_
#define __LOCATION_SCHEDULER_HPP_

#include <functional>

class LocationScheduler {
public:
	virtual ~LocationScheduler() {}
	virtual void start(std::function<void()> data_notification_callback = nullptr) = 0;
	virtual void stop() = 0;
	virtual void notify_saltwater_switch_state(bool state) = 0;
};

#endif // __LOCATION_SCHEDULER_HPP_
