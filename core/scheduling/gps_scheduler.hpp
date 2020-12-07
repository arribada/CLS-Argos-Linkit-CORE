#ifndef __GPS_SCHEDULER_HPP_
#define __GPS_SCHEDULER_HPP_

class GPSScheduler {
public:
	virtual ~GPSScheduler() {}
	virtual void start() = 0;
	virtual void stop() = 0;
	virtual void notify_saltwater_switch_state(bool state) = 0;
};

#endif // __GPS_SCHEDULER_HPP_
