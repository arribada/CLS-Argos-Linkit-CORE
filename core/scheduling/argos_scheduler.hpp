#pragma once

#include <variant>
#include <array>
#include <functional>
#include <random>
#include <ctime>

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
	ARGOS_3,
	ARGOS_4
};

static inline const char *argos_mode_to_string(ArgosMode mode) {
	if (mode == ArgosMode::ARGOS_2)
		return "ARGOS_2";
	if (mode == ArgosMode::ARGOS_3)
		return "ARGOS_3";
	if (mode == ArgosMode::ARGOS_4)
		return "ARGOS_4";
	return "UNKNOWN";
}

class ArgosScheduler : public ServiceScheduler {

public:
	enum class ArgosAsyncEvent {
		OFF,
		ON,
		TX_STARTED,
		TX_DONE,
		ACK_DONE,
		RX_PACKET,
		RX_TIMEOUT,
		ERROR
	};

	ArgosScheduler();
	void start(std::function<void(ServiceEvent&)> data_notification_callback = nullptr) override;
	void stop() override;
	void notify_saltwater_switch_state(bool state) override;
	void notify_sensor_log_update() override;
	uint64_t get_next_schedule();

	// These methods are specific to the chipset and should be implemented by device-specific subclass
	virtual void power_off() = 0;
	virtual void power_off_immediate() = 0;
	virtual void power_on(const unsigned int argos_id, std::function<void(ArgosAsyncEvent)> notification_callback) = 0;
	virtual void send_packet(ArgosPacket const& user_payload, unsigned int payload_length, const ArgosMode mode) = 0;
	virtual void read_packet(ArgosPacket& packet, unsigned int& size) = 0;
	virtual void set_rx_mode(const ArgosMode mode, const std::time_t stop_time) = 0;
	virtual uint64_t get_rx_time_on() = 0;
	virtual void set_frequency(const double freq) = 0;
	virtual void set_tx_power(const BaseArgosPower power) = 0;
	virtual void send_ack(const unsigned int a_dcs, const unsigned int dl_msg_id, const unsigned int exec_report, const ArgosMode mode) = 0;

private:
	Scheduler::TaskHandle m_tx_task;
	ArgosConfig  m_argos_config;
	bool         m_switch_state;
	bool         m_is_running;
	bool         m_is_deferred;
	bool         m_time_sync_burst_sent;
	uint64_t     m_earliest_tx;
	uint64_t     m_next_schedule_absolute;
	uint64_t     m_last_transmission_schedule;
	uint64_t     m_next_schedule_relative;
	ArgosMode    m_next_mode;
	unsigned int m_msg_index;
	unsigned int m_num_gps_entries;
	int          m_tx_jitter;
	std::mt19937 *m_rng;
	double		 m_last_longitude;
	double 		 m_last_latitude;
	std::map<unsigned int, unsigned int> m_gps_entry_burst_counter;
	std::map<unsigned int, GPSLogEntry> m_gps_log_entry;
	std::function<void(ServiceEvent&)> m_data_notification_callback;
	ArgosPacket  m_packet;
	unsigned int m_total_bits;
	ArgosMode    m_mode;
	uint64_t     m_downlink_start;
	uint64_t     m_downlink_end;
	bool         m_is_tx_pending;
	std::map<uint8_t, AopSatelliteEntry_t> m_orbit_params_map;
	std::map<uint8_t, AopSatelliteEntry_t> m_constellation_status_map;
	std::time_t  m_last_rx_time;

	void process_rx();
	void reschedule();
	void deschedule();
	void process_schedule();
	void update_tx_jitter(int min, int max);
	void prepare_certification_burst();
	void prepare_time_sync_burst();
	void prepare_normal_burst();
	void prepare_doppler_burst();
	void handle_packet(ArgosPacket const& packet, unsigned int total_bits, const ArgosMode mode);
	unsigned int convert_latitude(double x);
	unsigned int convert_longitude(double x);
	unsigned int build_certification_packet(ArgosPacket& packet);
	void build_doppler_packet(ArgosPacket& packet);
	void build_short_packet(GPSLogEntry const& gps_entry, ArgosPacket& packet);
	void build_long_packet(std::vector<GPSLogEntry> const& gps_entries, ArgosPacket& packet);
	void handle_event(ArgosAsyncEvent event);
	uint64_t next_duty_cycle(unsigned int duty_cycle);
	uint64_t next_prepass();
	uint64_t next_certification_tx();
	void handle_rx_packet();
	void update_pass_predict(BasePassPredict& pass_predict);
	void update_rx_time(void);
};
