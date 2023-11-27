#pragma once

#include <algorithm>
#include <vector>

#include "sensor.hpp"
#include "service.hpp"
#include "logger.hpp"

class SensorService : public Service {
public:
	SensorService(Sensor& sensor, ServiceIdentifier service, const char *name, Logger *logger) : Service(service, name, logger), m_sensor(sensor) {}
	virtual ~SensorService() {}

protected:
	Sensor &m_sensor;
	void sensor_handler(bool reschedule = true, bool gnss_shutdown = false) {
		try {
			if (m_gnss_active) {
				m_sample_number++;
				for (unsigned int chan = 0; chan < sensor_num_channels(); chan++) {
					m_samples[chan].push_back(m_sensor.read(chan));
				}
				service_complete(nullptr, nullptr, sensor_enable_tx_mode() != BaseSensorEnableTxMode::ONESHOT);
			} else if (gnss_shutdown && m_sample_number > 0) {
				ServiceSensorData sensor;
				for (unsigned int chan = 0; chan < sensor_num_channels(); chan++) {
					switch (sensor_enable_tx_mode()) {
					case BaseSensorEnableTxMode::ONESHOT:
						sensor.port[chan] = compute_oneshot_samples(m_samples[chan]);
						//DEBUG_TRACE("[%s] oneshot[%u]=%f", get_name(), chan, sensor.port[chan]);
						break;
					case BaseSensorEnableTxMode::MEAN:
						sensor.port[chan] = compute_mean_samples(m_samples[chan]);
						//DEBUG_TRACE("[%s] mean[%u]=%f", get_name(), chan, sensor.port[chan]);
						break;
					case BaseSensorEnableTxMode::MEDIAN:
						sensor.port[chan] = compute_median_samples(m_samples[chan]);
						//DEBUG_TRACE("[%s] median[%u]=%f", get_name(), chan, sensor.port[chan]);
						break;
					default:
					case BaseSensorEnableTxMode::OFF:
						break;
					}
				}
				LogEntry e;
				ServiceEventData data = sensor;
				sensor_populate_log_entry(&e, sensor);
				service_log(&data, &e);
			} else if (sensor_enable_tx_mode() == BaseSensorEnableTxMode::OFF) {
				ServiceSensorData sensor;
				for (unsigned int chan = 0; chan < sensor_num_channels(); chan++)
					sensor.port[chan] = m_sensor.read(chan);
				LogEntry e;
				ServiceEventData data = sensor;
				sensor_populate_log_entry(&e, sensor);
				service_log(&data, &e);
				service_complete(nullptr, nullptr, reschedule);
			} else {
				service_complete(nullptr, nullptr, reschedule);
			}
		} catch (ErrorCode e) {
			DEBUG_ERROR("[%s] Failed to read sensor [%04X]", get_name(), (unsigned int)e);
			if (!m_gnss_active)
				service_complete(nullptr, nullptr, reschedule);
		}
	}

private:
	std::vector<double> m_samples[5];
	unsigned int m_sample_number;
	bool m_gnss_active;

	double compute_mean_samples(std::vector<double>& v) {
		return std::reduce(v.begin(), v.end()) / v.size();
	}
	double compute_median_samples(std::vector<double>& v) {
		int middle = v.size() / 2;
		std::sort(v.begin(), v.end());
		return v.at(middle);
	}
	double compute_oneshot_samples(std::vector<double>& v) {
		return v.at(0);
	}

	void notify_peer_event(ServiceEvent& e) override {
		if (sensor_enable_tx_mode() != BaseSensorEnableTxMode::OFF) {
			handle_peer_event(e);
		}
		Service::notify_peer_event(e);
	}

	void handle_peer_event(ServiceEvent& e) {
		if (e.event_source == ServiceIdentifier::GNSS_SENSOR) {
			if (e.event_type == ServiceEventType::SERVICE_ACTIVE) {
				DEBUG_TRACE("%s: GNSS active - start sampling", get_name());
				m_gnss_active = true;
				m_sample_number = 0;
				reset_samples();
				service_reschedule(true);
			} else if (e.event_type == ServiceEventType::SERVICE_INACTIVE) {
				DEBUG_TRACE("%s: GNSS inactive - stop sampling", get_name());
				m_gnss_active = false;
				sensor_handler(false, true);
			}
		}
	}

	void service_initiate() override {
		sensor_handler();
	}

	unsigned int service_next_schedule_in_ms() override {
		if (m_gnss_active) {
			if (m_sample_number == 0) return 0U;
			return sensor_tx_periodic();
		} else {
			return sensor_periodic();
		}
	}

	bool service_is_enabled() override {
		return sensor_is_enabled();
	}

	void service_init() override {
		m_gnss_active = false;
		m_sample_number = 0;
		reset_samples();
		sensor_init();
	}

	void reset_samples() {
		for (unsigned int chan = 0; chan < sensor_num_channels(); chan++)
			m_samples[chan].clear();
	}

	void service_term() override { sensor_term(); }
	bool service_is_usable_underwater() override { return sensor_is_usable_underwater(); }
	unsigned int service_next_timeout() override { return 0U; }

	// Sensor service should implements these functions
	virtual void sensor_init() {}
	virtual void sensor_term() {}
	virtual bool sensor_is_enabled() { return false; }
	virtual BaseSensorEnableTxMode sensor_enable_tx_mode() {
		return BaseSensorEnableTxMode::OFF;
	}
	virtual void sensor_populate_log_entry(LogEntry *e, ServiceSensorData& data) = 0;
	virtual unsigned int sensor_periodic() { return 1000U; }
	virtual unsigned int sensor_tx_periodic() { return 1000U; }
	virtual unsigned int sensor_max_samples() { return 1U; }
	virtual unsigned int sensor_num_channels() { return 1U; }
	virtual bool sensor_is_usable_underwater() { return true; }
};
