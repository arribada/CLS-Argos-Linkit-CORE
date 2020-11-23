#ifndef __GPS_SCHEDULER_HPP_
#define __GPS_SCHEDULER_HPP_

#include "config_store.hpp"
#include "sensor_log.hpp"
#include "system_log.hpp"


class GPSScheduler {
public:
	GPSScheduler(ConfigurationStore *config_store, SensorLog *sensor_log, SystemLog *sys_log);
	void notify_saltwater_switch_state(bool);
};

#endif // __GPS_SCHEDULER_HPP_
