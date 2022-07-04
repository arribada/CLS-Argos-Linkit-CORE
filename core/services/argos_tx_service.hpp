#pragma once

#include <ctime>
#include <optional>
#include <deque>
#include <vector>
#include <random>
#include "artic_device.hpp"
#include "service.hpp"
#include "config_store.hpp"


template<typename T> class ArgosDepthPile {
private:
	struct Entry {
		unsigned int burst_counter;
		T data;
	public:
		Entry(T& d, unsigned int c) {
			data = d;
			burst_counter = c;
		}
	};

	std::deque<Entry> m_entry;
	unsigned int m_max_size;
	unsigned int m_retrieve_index;

public:
	ArgosDepthPile(unsigned int max_size=24) : m_max_size(max_size), m_retrieve_index(0) {}

	void clear() {
		m_entry.clear();
	}

	void store(T& e, unsigned int burst_count) {
		m_entry.push_back(Entry(e, burst_count));
		if (m_entry.size() > m_max_size)
			m_entry.pop_front();
		DEBUG_TRACE("ArgosDepthPile::store: depth pile has %u/%u entries", m_entry.size(), m_max_size);
	}

	unsigned int size() {
		return m_entry.size();
	}

	unsigned int eligible() {
		unsigned int count = 0;
		for (auto const& it : m_entry) {
			if (it.burst_counter) count++;
		}
		return count;
	}

	std::vector<T*> retrieve_latest() {
		std::vector<T*> v;
		if (m_entry.size()) {
			unsigned int idx = m_entry.size()-1;
			if (m_entry[idx].burst_counter)
				v.push_back(&m_entry[idx].data);
		}
		return v;
	}

	std::vector<T*> retrieve(unsigned int depth, unsigned int max_messages=4) {
		max_messages = std::min(depth, max_messages);
		unsigned int max_index = (depth + (max_messages-1)) / max_messages;
		unsigned int span = std::min(max_messages, (unsigned int)m_entry.size());
		std::vector<T*> v;

		DEBUG_TRACE("ArgosDepthPile: retrieve: slot=%u/%u span=%u occupancy=%u", m_retrieve_index % max_index, max_index-1, span, m_entry.size());

		// Find first eligible slot for transmission
		unsigned int max_msg_index = m_retrieve_index + max_index;
		unsigned int retrieve_index = 0;
		unsigned int eligible = 0;
		std::optional<unsigned int> first_eligible;
		while (m_retrieve_index < max_msg_index && !eligible) {
			retrieve_index = m_retrieve_index % max_index;
			// Check to see if any GPS entry has a non-zero burst counter
			for (unsigned int k = 0; k < span; k++) {
				unsigned int idx = m_entry.size() - (span * (retrieve_index+1)) + k;
				if (idx < m_entry.size() && m_entry[idx].burst_counter) {
					eligible++;
					if (!first_eligible.has_value())
						first_eligible = idx;
				}
			}

			m_retrieve_index++;
		}

		if (eligible == 1) {
			DEBUG_TRACE("ArgosDepthPile: retrieve: idx=%u burst_counter=%u", first_eligible.value(), m_entry[first_eligible.value()].burst_counter);
			m_entry[first_eligible.value()].burst_counter--;
			v.push_back(&m_entry[first_eligible.value()].data);
		} else if (eligible > 1) {
			for (unsigned int k = 0; k < span; k++) {
				unsigned int idx = m_entry.size() - (span * (retrieve_index+1)) + k;
				DEBUG_TRACE("ArgosDepthPile: retrieve: idx=%u burst_counter=%u", idx, m_entry[idx].burst_counter);
				// We may have zero burst counter in some entries
				if (m_entry[idx].burst_counter)
					m_entry[idx].burst_counter--;
				v.push_back(&m_entry[idx].data);
			}
		} else {
			DEBUG_TRACE("ArgosDepthPile: retrieve: no eligible entries found");
		}

		return v;
	}
};

class ArgosPacketBuilder {
public:
	static inline const unsigned int SHORT_PACKET_BITS   		 = 120;
	static inline const unsigned int SHORT_PACKET_PAYLOAD_BITS   = 99;
	static inline const unsigned int SHORT_PACKET_BYTES			 = 15;

	static inline const unsigned int LONG_PACKET_BITS   		 = 248;
	static inline const unsigned int LONG_PACKET_PAYLOAD_BITS    = 216;
	static inline const unsigned int LONG_PACKET_BYTES			 = 31;

	static inline const unsigned int DOPPLER_PACKET_BITS   		 = 24;
	static inline const unsigned int DOPPLER_PACKET_PAYLOAD_BITS = 24;
	static inline const unsigned int DOPPLER_PACKET_BYTES		 = 3;

	static inline const unsigned int FIXTYPE_3D			 = 3;
	static inline const unsigned int HOURS_PER_DAY       = 24;
	static inline const unsigned int SECONDS_PER_MINUTE	 = 60;
	static inline const unsigned int SECONDS_PER_HOUR    = 3600;
	static inline const unsigned int SECONDS_PER_DAY     = (SECONDS_PER_HOUR * HOURS_PER_DAY);
	static inline const unsigned int MM_PER_METER		 = 1000;
	static inline const unsigned int MM_PER_KM   	  	 = 1000000;
	static inline const unsigned int MV_PER_UNIT		 = 20;
	static inline const unsigned int MS_PER_SEC			 = 1000;
	static inline const unsigned int METRES_PER_UNIT     = 40;
	static inline const unsigned int DEGREES_PER_UNIT	 = (1.0f/1.42f);
	static inline const unsigned int BITS_PER_BYTE		 = 8;
	static inline const unsigned int MIN_ALTITUDE		 = 0;
	static inline const unsigned int MAX_ALTITUDE		 = 254;
	static inline const unsigned int INVALID_ALTITUDE	 = 255;
	static inline const unsigned int REF_BATT_MV	 	 = 2700;
	static inline const unsigned int LON_LAT_RESOLUTION  = 10000;
	static inline const unsigned int MAX_GPS_ENTRIES_IN_PACKET = 4;

	static unsigned int convert_altitude(double x);
	static unsigned int convert_heading(double x);
	static unsigned int convert_speed(double x);
	static unsigned int convert_latitude(double x);
	static unsigned int convert_longitude(double x);
	static unsigned int convert_battery_voltage(unsigned int battery_voltage);
	static ArticPacket build_short_packet(GPSLogEntry* v,
			bool is_out_of_zone,
			bool is_low_battery);
	static ArticPacket build_long_packet(std::vector<GPSLogEntry*> &v,
			bool is_out_of_zone,
			bool is_low_battery,
			BaseDeltaTimeLoc delta_time_loc);
	static ArticPacket build_gnss_packet(std::vector<GPSLogEntry*> &v,
			bool is_out_of_zone,
			bool is_low_battery,
			BaseDeltaTimeLoc delta_time_loc,
			unsigned int &size_bits);
	static ArticPacket build_certification_packet(std::string cert_tx_payload, unsigned int &size_bits);
	static ArticPacket build_doppler_packet(unsigned int battery, bool is_low_battery, unsigned int &size_bits);
};

class ArgosTxScheduler {
private:
	struct Location {
		double longitude;
		double latitude;
		Location(double x, double y) {
			longitude = x;
			latitude = y;
		}
	};
	std::optional<uint64_t> m_last_schedule_abs;
	std::optional<uint64_t> m_curr_schedule_abs;
	std::optional<uint64_t> m_earliest_schedule;
	std::mt19937 m_rand;
	std::optional<Location> m_location;

	static inline const unsigned int SECONDS_PER_MINUTE = 60;
	static inline const unsigned int MINUTES_PER_HOUR   = 60;
	static inline const unsigned int HOURS_PER_DAY      = 24;
	static inline const unsigned int MSECS_PER_SECOND   = 1000;
	static inline const unsigned int SECONDS_PER_HOUR   = MINUTES_PER_HOUR * SECONDS_PER_MINUTE;
	static inline const unsigned int SECONDS_PER_DAY    = HOURS_PER_DAY * SECONDS_PER_HOUR;
	static inline const unsigned int DUTYCYCLE_24HRS    = 0xFFFFFFU;
	static inline const unsigned int ARGOS_TX_MARGIN_MSECS = 0;

	int compute_random_jitter(bool jitter_en, int min = -5000, int max = 5000);
	void schedule_periodic(unsigned int period_ms, bool jitter_en, unsigned int duty_cycle, uint64_t now_ms);

public:
	static inline const unsigned int INVALID_SCHEDULE   = (unsigned int)-1;
	static bool is_in_duty_cycle(uint64_t time_ms, unsigned int duty_cycle);

	ArgosTxScheduler();
	unsigned int schedule_prepass(ArgosConfig& config, BasePassPredict& pass_predict, ArticMode& scheduled_mode, std::time_t now);
	unsigned int schedule_duty_cycle(ArgosConfig& config, std::time_t now);
	unsigned int schedule_legacy(ArgosConfig& config, std::time_t now);
	void set_earliest_schedule(std::time_t t);
	void set_last_location(double lon, double lat);
	unsigned int get_last_schedule();
	void reset(unsigned int seed);
	void schedule_at(std::time_t t);
	void notify_tx_complete();
};


class ArgosTxService : public Service, ArticEventListener {
public:
	ArgosTxService(ArticDevice& device);
	void notify_peer_event(ServiceEvent& e) override;

protected:
	// Service interface methods
	void service_init() override;
	void service_term() override;
	bool service_is_enabled() override;
	unsigned int service_next_schedule_in_ms() override;
	void service_initiate() override;
	bool service_cancel() override;
	bool service_is_triggered_on_surfaced(bool &immediate) override;
	bool service_is_active_on_initiate() override;

private:
	ArticDevice& m_artic;
	ArgosDepthPile<GPSLogEntry> m_gps_depth_pile;
	ArgosTxScheduler m_sched;
	bool m_is_first_tx;
	bool m_is_tx_pending;
	std::function<void()> m_scheduled_task;
	ArticMode m_scheduled_mode;

	void react(ArticEventTxStarted const &) override;
	void react(ArticEventTxComplete const &) override;
	void react(ArticEventDeviceError const &) override;

	void process_certification_burst();
	void process_time_sync_burst();
	void process_gnss_burst();
	void process_doppler_burst();
};
