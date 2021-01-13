#ifndef __COMMS_SCHEDULER_HPP_
#define __COMMS_SCHEDULER_HPP_

class CommsScheduler {
public:
	virtual ~CommsScheduler() {}
	virtual void start() = 0;
	virtual void stop() = 0;
	virtual void notify_saltwater_switch_state(bool state) = 0;
	virtual void notify_sensor_log_update() = 0;
};

#endif // __COMMS_SCHEDULER_HPP_
