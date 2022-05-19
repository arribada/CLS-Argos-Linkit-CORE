#include "argos_rx_service.hpp"
#include "config_store.hpp"
#include "binascii.hpp"
#include "dte_protocol.hpp"

extern ConfigurationStore *configuration_store;

#define SECONDS_PER_HOUR   3600

ArgosRxService::ArgosRxService(ArticDevice& device) : Service(ServiceIdentifier::ARGOS_RX, "ARGOSRX"), m_artic(device) {
}

void ArgosRxService::service_init() {
	ArgosConfig argos_config;
	configuration_store->get_argos_configuration(argos_config);
	m_artic.set_frequency(argos_config.frequency);
	m_artic.set_tcxo_warmup_time(argos_config.argos_tcxo_warmup_time);
	m_artic.subscribe(*this);
}

void ArgosRxService::service_term() {
	m_artic.unsubscribe(*this);
}

bool ArgosRxService::service_is_enabled() {
	ArgosConfig argos_config;
	configuration_store->get_argos_configuration(argos_config);
	return (argos_config.argos_rx_en && argos_config.mode == BaseArgosMode::PASS_PREDICTION && !argos_config.cert_tx_enable);
}

unsigned int ArgosRxService::service_next_schedule_in_ms() {
	ArgosConfig argos_config;
	configuration_store->get_argos_configuration(argos_config);
	BasePassPredict& pass_predict = configuration_store->read_pass_predict();
	std::time_t now = service_current_time();
	return m_sched.schedule(argos_config, pass_predict, now, m_timeout, m_mode);
}

void ArgosRxService::service_initiate() {
	DEBUG_INFO("ArgosRxService::service_initiate: starting RX");
	m_artic.start_receive(m_mode);
}

bool ArgosRxService::service_cancel() {
	m_artic.stop_receive();
	unsigned int t = m_artic.get_cumulative_receive_time();
	DEBUG_TRACE("ArgosRxService::service_cancel: pending=%u", t ? true : false);
	if (t) {
		DEBUG_INFO("ArgosRxService::service_cancel: stopped RX");
		configuration_store->increment_rx_time(t);
		configuration_store->save_params();
		return true;
	}
	return false;
}

unsigned int ArgosRxService::service_next_timeout() {
	return m_timeout;
}

void ArgosRxService::notify_peer_event(ServiceEvent& e) {

	if (e.event_source == ServiceIdentifier::GNSS_SENSOR &&
		e.event_type == ServiceEventType::SERVICE_LOG_UPDATED)
	{
		// Update location information if we got a valid fix
		GPSLogEntry& gps = std::get<GPSLogEntry>(e.event_data);
		if (gps.info.valid) {
			DEBUG_TRACE("ArgosRxService::notify_peer_event: updated GPS location");
			m_sched.set_location(gps.info.lon, gps.info.lat);
			service_reschedule();
		}
	} else if (e.event_source == ServiceIdentifier::UW_SENSOR && e.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
		if (std::get<bool>(e.event_data) == false) {
			ArgosConfig argos_config;
			configuration_store->get_argos_configuration(argos_config);
			std::time_t earliest_schedule = service_current_time() + argos_config.dry_time_before_tx;
			m_sched.set_earliest_schedule(earliest_schedule);
		}
	}

	Service::notify_peer_event(e);
}

bool ArgosRxService::service_is_triggered_on_surfaced(bool& immediate) {
	immediate = false;
	return true;
}

void ArgosRxService::react(ArticEventRxPacket const& e) {
	DEBUG_INFO("ArgosScheduler::handle_rx_packet: packet=%s length=%u", Binascii::hexlify(e.packet).c_str(), e.size_bits);

	// Increment RX counter
	configuration_store->increment_rx_counter();

	// Save configuration params
	configuration_store->save_params();

	// Attempt to decode the queue of packets
	BasePassPredict pass_predict;
	PassPredictCodec::decode(m_orbit_params_map, m_constellation_status_map, e.packet, pass_predict);

	// Check to see if any new AOP records were found
	if (pass_predict.num_records)
		update_pass_predict(pass_predict);
}

void ArgosRxService::react(ArticEventDeviceError const&) {
	service_cancel();
	service_complete();
}

void ArgosRxService::update_pass_predict(BasePassPredict& new_pass_predict) {

	BasePassPredict existing_pass_predict;
	unsigned int num_updated_records = 0;

	// Read in the existing pass predict database
	existing_pass_predict = configuration_store->read_pass_predict();

	// Iterate over new candidate records
	for (unsigned int i = 0; i < new_pass_predict.num_records; i++) {
		unsigned int j = 0;

		for (; j < existing_pass_predict.num_records; j++) {
			// Check for existing hex ID match
			if (new_pass_predict.records[i].satHexId == existing_pass_predict.records[j].satHexId) {
				DEBUG_TRACE("ArgosRxService::update_pass_predict: hexid=%01x dl=%u ul=%u aop=%u",
						(unsigned int)new_pass_predict.records[i].satHexId,
						(unsigned int)new_pass_predict.records[i].downlinkStatus,
						(unsigned int)new_pass_predict.records[i].uplinkStatus,
						(unsigned int)new_pass_predict.records[i].bulletin.year ? 1: 0
						);
				if ((new_pass_predict.records[i].downlinkStatus || new_pass_predict.records[i].uplinkStatus) &&
						new_pass_predict.records[i].bulletin.year) {
					num_updated_records++;
					existing_pass_predict.records[j] = new_pass_predict.records[i];
				} else if (!new_pass_predict.records[i].downlinkStatus && !new_pass_predict.records[i].uplinkStatus) {
					num_updated_records++;
					existing_pass_predict.records[j].downlinkStatus = new_pass_predict.records[i].downlinkStatus;
					existing_pass_predict.records[j].uplinkStatus = new_pass_predict.records[i].uplinkStatus;
				}
				break;
			}
		}

		// If we reached the end of the existing database then this is a new hex ID, so
		// add it to the end of the existing database
		if (j == existing_pass_predict.num_records &&
			existing_pass_predict.num_records < MAX_AOP_SATELLITE_ENTRIES) {
			if ((new_pass_predict.records[i].downlinkStatus || new_pass_predict.records[i].uplinkStatus) &&
					new_pass_predict.records[i].bulletin.year) {
				existing_pass_predict.records[j] = new_pass_predict.records[i];
				existing_pass_predict.num_records++;
				num_updated_records++;
			} else if (!new_pass_predict.records[i].downlinkStatus && !new_pass_predict.records[i].uplinkStatus) {
				existing_pass_predict.records[j].downlinkStatus = new_pass_predict.records[i].downlinkStatus;
				existing_pass_predict.records[j].uplinkStatus = new_pass_predict.records[i].uplinkStatus;
				existing_pass_predict.num_records++;
				num_updated_records++;
			}
		}
	}

	DEBUG_TRACE("ArgosRxService::update_pass_predict: received=%u required=%u", num_updated_records, existing_pass_predict.num_records);

	// Check if we received a sufficient number of records
	if (num_updated_records == new_pass_predict.num_records && num_updated_records >= existing_pass_predict.num_records) {
		DEBUG_INFO("ArgosRxService::update_pass_predict: committing %u AOP records", num_updated_records);
		configuration_store->write_pass_predict(existing_pass_predict);
		std::time_t new_aop_time = service_current_time();
		configuration_store->write_param(ParamID::ARGOS_AOP_DATE, new_aop_time);
		configuration_store->save_params();
		m_orbit_params_map.clear();
		m_constellation_status_map.clear();
		service_cancel();
		service_complete();
	}
}

ArgosRxScheduler::ArgosRxScheduler() : m_earliest_schedule(0) {
	m_location.reset();
}

unsigned int ArgosRxScheduler::schedule(ArgosConfig& argos_config, BasePassPredict& pass_predict, std::time_t now, unsigned int &timeout, ArticMode& mode) {
	if (!m_location.has_value()) {
		DEBUG_TRACE("ArgosRxService::schedule: can't schedule as last location/time is not known");
		return Service::SCHEDULE_DISABLED;
	}

	// Update earliest schedule according to configuration and current time
	set_earliest_schedule(argos_config.last_aop_update + (SECONDS_PER_DAY * argos_config.argos_rx_aop_update_period));
	set_earliest_schedule(now);

	std::time_t start_time = m_earliest_schedule;
	std::time_t stop_time = start_time + (std::time_t)SECONDS_PER_DAY;
	struct tm *p_tm = std::gmtime(&start_time);
	struct tm tm_start = *p_tm;
	p_tm = std::gmtime(&stop_time);
	struct tm tm_stop = *p_tm;

	DEBUG_TRACE("ArgosRxService::service_next_schedule_in_ms: searching window start=%llu stop=%llu", start_time, stop_time);

	PredictionPassConfiguration_t pp_config = {
		(float)m_location.value().latitude,
		(float)m_location.value().longitude,
		{ (uint16_t)(1900 + tm_start.tm_year), (uint8_t)(tm_start.tm_mon + 1), (uint8_t)tm_start.tm_mday, (uint8_t)tm_start.tm_hour, (uint8_t)tm_start.tm_min, (uint8_t)tm_start.tm_sec },
		{ (uint16_t)(1900 + tm_stop.tm_year), (uint8_t)(tm_stop.tm_mon + 1), (uint8_t)tm_stop.tm_mday, (uint8_t)tm_stop.tm_hour, (uint8_t)tm_stop.tm_min, (uint8_t)tm_stop.tm_sec },
        (float)argos_config.prepass_min_elevation,        //< Minimum elevation of passes [0, 90]
		(float)argos_config.prepass_max_elevation,        //< Maximum elevation of passes  [maxElevation >= < minElevation]
		(float)argos_config.prepass_min_duration / 60.0f,  //< Minimum duration (minutes)
		argos_config.prepass_max_passes,                  //< Maximum number of passes per satellite (#)
		(float)argos_config.prepass_linear_margin / 60.0f, //< Linear time margin (in minutes/6months)
		argos_config.prepass_comp_step                    //< Computation step (seconds)
	};
	SatelliteNextPassPrediction_t next_pass;

	while (PREVIPASS_compute_next_pass_with_status(
    	&pp_config,
		pass_predict.records,
		pass_predict.num_records,
		SAT_DNLK_ON_WITH_A3,
		SAT_UPLK_OFF,
		&next_pass)) {

		// Set initial start/end points based on this discovered window
		std::time_t start = start_time, end = next_pass.epoch + next_pass.duration;

		// Advance to at least the prepass epoch position
		start = std::max((std::time_t)next_pass.epoch, start);

		DEBUG_TRACE("ArgosRxScheduler::schedule_prepass: sat=%01x dl=%u e=%llu t=%llu [%llu %llu %llu]",
					(unsigned int)next_pass.satHexId,
					(unsigned int)next_pass.downlinkStatus,
					m_earliest_schedule,
					now,
					start_time,
					next_pass.epoch,
					end);

		// Check we don't schedule off the end of the computed window
		if ((start + ARGOS_RX_MARGIN_MSECS) < end) {
			// We're good to go for this schedule, compute relative delay until the epoch arrives
			DEBUG_TRACE("ArgosRxScheduler::schedule_prepass: scheduled for %llu seconds from now", start - now);
			mode = ArticMode::A3;
			timeout = (end - start) * MSECS_PER_SECOND;
			return start - now;
		} else {
			break;
		}
	}

	DEBUG_ERROR("ArgosRxService::schedule: failed to find DL RX window");
	return Service::SCHEDULE_DISABLED;
}

void ArgosRxScheduler::set_earliest_schedule(std::time_t t) {
	if (t > m_earliest_schedule) {
		DEBUG_TRACE("ArgosRxScheduler::set_earliest_schedule: new earliest: %llu", t);
		m_earliest_schedule = t;
	}
}

void ArgosRxScheduler::set_location(double lon, double lat) {
	m_location = Location(lon, lat);
}
