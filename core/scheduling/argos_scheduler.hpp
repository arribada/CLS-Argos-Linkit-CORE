#ifndef __ARGOS_SCHEDULER_HPP_
#define __ARGOS_SCHEDULER_HPP_

#include "config_store.hpp"
#include "sensor_log.hpp"
#include "system_log.hpp"

class ArgosScheduler {
public:
	ArgosScheduler(ConfigurationStore *config_store, SensorLog *sensor_log, SystemLog *sys_log);
	void notify_saltwater_switch_state(bool);
};

#endif // __ARGOS_SCHEDULER_HPP_
