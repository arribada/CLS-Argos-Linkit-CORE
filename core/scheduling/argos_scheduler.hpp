#ifndef __ARGOS_SCHEDULER_HPP_
#define __ARGOS_SCHEDULER_HPP_

#include <variant>
#include <array>
#include <functional>

#include <stdint.h>

#include "rtc.hpp"
#include "service_scheduler.hpp"
#include "scheduler.hpp"
#include "config_store.hpp"


#define MAX_MSG_INDEX    6
#define INVALID_GEODESIC -1000

using ArgosPacket = std::string;

enum class ArgosMode {
	ARGOS_2,
	ARGOS_3
};

class ArgosScheduler : public ServiceScheduler {
private:
	Scheduler::TaskHandle m_argos_task;
	ArgosConfig  m_argos_config;
	bool         m_switch_state;
	bool         m_is_running;
	std::time_t  m_earliest_tx;
	std::time_t  m_next_prepass;
	ArgosMode    m_next_mode;
	unsigned int m_msg_index;
	double		 m_last_longitude;
	double 		 m_last_latitude;
	std::array<unsigned int, MAX_MSG_INDEX> m_msg_burst_counter;
	std::array<std::vector<GPSLogEntry>, MAX_MSG_INDEX> m_gps_entries;
	bool		 m_is_rtc_set;

public:
	ArgosScheduler();
	void start(std::function<void()> data_notification_callback = nullptr) override;
	void stop() override;
	void notify_saltwater_switch_state(bool state) override;
	void notify_sensor_log_update() override;

	void reschedule();
	void deschedule();
	void process_schedule();
	void periodic_algorithm();
	void pass_prediction_algorithm();
	void handle_packet(ArgosPacket const& packet, unsigned int total_bits, const ArgosMode mode);
	unsigned int convert_latitude(double x);
	unsigned int convert_longitude(double x);
	void build_short_packet(GPSLogEntry const& gps_entry, ArgosPacket& packet);
	void build_long_packet(std::vector<GPSLogEntry> const& gps_entries, ArgosPacket& packet);
	BaseDeltaTimeLoc delta_time_loc(GPSLogEntry const& a, GPSLogEntry const& b);
	std::time_t next_duty_cycle(unsigned int duty_cycle);
	std::time_t next_prepass();

	// These methods are specific to the chipset and should be implemented by device-specific subclass
	virtual void power_off() = 0;
	virtual void power_on() = 0;
	virtual void send_packet(ArgosPacket const& packet, unsigned int total_bits, const ArgosMode mode) = 0;
	virtual void set_frequency(const double freq) = 0;
	virtual void set_tx_power(const BaseArgosPower power) = 0;
};

#endif // __ARGOS_SCHEDULER_HPP_
