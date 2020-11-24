#ifndef __GPS_SCHEDULER_HPP_
#define __GPS_SCHEDULER_HPP_


class GPSScheduler {
public:
	static void start();
	static void stop();
	static void notify_saltwater_switch_state(bool);
};

#endif // __GPS_SCHEDULER_HPP_
