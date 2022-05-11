#include <climits>
#include <algorithm>

#include "argos_tx_service.hpp"
#include "messages.hpp"
#include "timeutils.hpp"
#include "bitpack.hpp"
#include "debug.hpp"
#include "crc8.hpp"
#include "bch.hpp"
#include "binascii.hpp"


extern ConfigurationStore *configuration_store;


ArgosTxService::ArgosTxService(ArticDevice& device) : Service(ServiceIdentifier::ARGOS_TX, "ARGOSTX"),
	m_artic(device)
{
}

void ArgosTxService::service_init() {
	ArgosConfig argos_config;
	configuration_store->get_argos_configuration(argos_config);
	m_artic.set_frequency(argos_config.frequency);
	m_artic.set_tcxo_warmup_time(argos_config.argos_tcxo_warmup_time);
	m_artic.set_tx_power(argos_config.power);
	m_artic.set_device_identifier(argos_config.argos_id);
	m_artic.subscribe(*this);
	m_sched.reset(argos_config.argos_id);
	m_gps_depth_pile.clear();
	if (argos_config.cert_tx_enable)
		m_artic.set_idle_timeout(10000);
	m_is_first_tx = true;
}

void ArgosTxService::service_term() {
	m_artic.unsubscribe(*this);
}

bool ArgosTxService::service_is_enabled() {
	ArgosConfig argos_config;
	configuration_store->get_argos_configuration(argos_config);
	return (argos_config.mode != BaseArgosMode::OFF || argos_config.cert_tx_enable);
}

unsigned int ArgosTxService::service_next_schedule_in_ms() {
	ArgosConfig argos_config;
	configuration_store->get_argos_configuration(argos_config);
	std::time_t now = service_current_time();

	DEBUG_TRACE("ArgosTxService::service_next_schedule_in_ms");

	if (argos_config.cert_tx_enable) {
		m_scheduled_mode = (ArticMode)argos_config.cert_tx_modulation;
		m_scheduled_task = [this]() { process_certification_burst(); };
		unsigned int delta = m_is_first_tx ? 0 : argos_config.cert_tx_repetition * 1000;
		m_sched.schedule_at(now + delta);
		return delta;
	} else {
		if (argos_config.mode == BaseArgosMode::OFF) {
			return Service::SCHEDULE_DISABLED;
		} else {
			if (!argos_config.gnss_en) {
				m_scheduled_task = [this]() { process_doppler_burst(); };
				if (argos_config.mode == BaseArgosMode::DUTY_CYCLE) {
					m_scheduled_mode = ArticMode::A2;
					return m_sched.schedule_duty_cycle(argos_config, now);
				}
				if (argos_config.mode == BaseArgosMode::LEGACY) {
					m_scheduled_mode = ArticMode::A2;
					return m_sched.schedule_legacy(argos_config, now);
				}
				return Service::SCHEDULE_DISABLED;
			}
			if (m_gps_depth_pile.eligible() == 0) {
				DEBUG_TRACE("ArgosTxService::service_next_schedule_in_ms: depth pile has no eligible entries");
				return Service::SCHEDULE_DISABLED;
			}
			if (m_is_first_tx && argos_config.time_sync_burst_en) {
				m_scheduled_mode = ArticMode::A2;
				m_scheduled_task = [this]() { process_time_sync_burst(); };
				m_sched.schedule_at(now);
				return 0;
			}
			if (argos_config.mode == BaseArgosMode::DUTY_CYCLE) {
				m_scheduled_mode = ArticMode::A2;
				m_scheduled_task = [this]() { process_gnss_burst(); };
				return m_sched.schedule_duty_cycle(argos_config, now);
			}
			if (argos_config.mode == BaseArgosMode::LEGACY) {
				m_scheduled_mode = ArticMode::A2;
				m_scheduled_task = [this]() { process_gnss_burst(); };
				return m_sched.schedule_legacy(argos_config, now);
			}
			if (argos_config.mode == BaseArgosMode::PASS_PREDICTION) {
				m_scheduled_task = [this]() { process_gnss_burst(); };
				BasePassPredict pass_predict = configuration_store->read_pass_predict();
				return m_sched.schedule_prepass(argos_config, pass_predict, m_scheduled_mode, now);
			}
		}
	}

	return Service::SCHEDULE_DISABLED;
}

void ArgosTxService::service_initiate() {
	DEBUG_TRACE("ArgosTxService::service_initiate");
	m_is_first_tx = false;
	m_is_tx_pending = true;
	m_scheduled_task();
}

bool ArgosTxService::service_is_active_on_initiate() {
	return false;
}

bool ArgosTxService::service_cancel() {
	DEBUG_TRACE("ArgosTxService::service_cancel: pending=%u", m_is_tx_pending);
	bool is_pending = m_is_tx_pending;
	m_is_tx_pending = false;
	m_artic.stop_send();
	return is_pending;
}

bool ArgosTxService::service_is_triggered_on_surfaced(bool &immediate) {
	immediate = false;
	return true;  // Re-schedule us on a surfaced event
}

void ArgosTxService::notify_peer_event(ServiceEvent& e) {
	DEBUG_TRACE("ArgosTxService::notify_peer_event");

	if (e.event_source == ServiceIdentifier::GNSS_SENSOR &&
		e.event_type == ServiceEventType::SENSOR_LOG_UPDATED)
	{
		GPSLogEntry& entry = std::get<GPSLogEntry>(e.event_data);
		ArgosConfig argos_config;
		configuration_store->get_argos_configuration(argos_config);

		// Store the entry into the depth pile
		unsigned int burst_counter = (argos_config.ntry_per_message == 0 ||
				argos_config.mode == BaseArgosMode::DUTY_CYCLE ||
				argos_config.mode == BaseArgosMode::LEGACY) ? UINT_MAX : argos_config.ntry_per_message;
		m_gps_depth_pile.store(entry, burst_counter);

		// Update last known location
		if (entry.info.valid) {
			DEBUG_TRACE("ArgosTxService::notify_peer_event: updated GPS location");
			m_sched.set_last_location(entry.info.lon, entry.info.lat);
			if (!service_is_scheduled()) {
				// Reschedule service if we have no existing schedule which may be necessary
				// in the situation that the GPS location was not known in prepass mode
				service_reschedule();
			}
		}
	} else if (e.event_source == ServiceIdentifier::UW_SENSOR) {
		if (std::get<bool>(e.event_data) == false) {
			ArgosConfig argos_config;
			configuration_store->get_argos_configuration(argos_config);
			std::time_t earliest_schedule = service_current_time() + argos_config.dry_time_before_tx;
			m_sched.set_earliest_schedule(earliest_schedule);
		}
	}

	Service::notify_peer_event(e);
}

void ArgosTxService::process_certification_burst() {
	DEBUG_TRACE("ArgosTxService::process_doppler_burst");
	ArgosConfig argos_config;
	configuration_store->get_argos_configuration(argos_config);
	unsigned int size_bits;
	ArticPacket packet = ArgosPacketBuilder::build_certification_packet(argos_config.cert_tx_payload, size_bits);
	DEBUG_TRACE("ArgosTxService::process_certification_burst: mode=%s data=%s sz=%u", argos_modulation_to_string(argos_config.cert_tx_modulation), Binascii::hexlify(packet).c_str(), size_bits);
	m_artic.send((ArticMode)argos_config.cert_tx_modulation, packet, size_bits);
}

void ArgosTxService::process_time_sync_burst() {
	DEBUG_TRACE("ArgosTxService::process_time_sync_burst");
	ArgosConfig argos_config;
	configuration_store->get_argos_configuration(argos_config);
	unsigned int size_bits;
	std::vector<GPSLogEntry*> v = m_gps_depth_pile.retrieve_latest();
	if (v.size()) {
		ArticPacket packet = ArgosPacketBuilder::build_gnss_packet(v, argos_config.is_out_of_zone, argos_config.is_lb,
				argos_config.delta_time_loc,
				size_bits);
		DEBUG_TRACE("ArgosTxService::process_time_sync_burst: mode=A2 data=%s sz=%u", Binascii::hexlify(packet).c_str(), size_bits);
		m_artic.send(ArticMode::A2, packet, size_bits);
	} else {
		// No eligible entries for transmission in the depth pile, so send a doppler burst instead
		DEBUG_WARN("ArgosTxService::process_time_sync_burst: no entries eligible in depth pile");
		process_doppler_burst();
	}
}

void ArgosTxService::process_gnss_burst() {
	DEBUG_TRACE("ArgosTxService::process_gnss_burst");
	ArgosConfig argos_config;
	configuration_store->get_argos_configuration(argos_config);
	unsigned int size_bits;
	std::vector<GPSLogEntry*> v = m_gps_depth_pile.retrieve((unsigned int)argos_config.depth_pile);
	if (v.size()) {
		ArticPacket packet = ArgosPacketBuilder::build_gnss_packet(v, argos_config.is_out_of_zone, argos_config.is_lb,
				argos_config.delta_time_loc,
				size_bits);
		DEBUG_TRACE("ArgosTxService::process_gnss_burst");
		m_artic.send(m_scheduled_mode, packet, size_bits);
	} else {
		// No eligible entries for transmission in the depth pile, so send a doppler burst instead
		DEBUG_WARN("ArgosTxService::process_gnss_burst: no entries eligible in depth pile");
		process_doppler_burst();
	}
}

void ArgosTxService::process_doppler_burst() {
	DEBUG_TRACE("ArgosTxService::process_doppler_burst");
	unsigned int size_bits;
	ArticPacket packet = ArgosPacketBuilder::build_doppler_packet(service_get_voltage(), service_is_battery_level_low(), size_bits);
	DEBUG_TRACE("ArgosTxService::process_doppler_burst: mode=A2 data=%s sz=%u", Binascii::hexlify(packet).c_str(), size_bits);
	m_artic.send(ArticMode::A2, packet, size_bits);
}

void ArgosTxService::react(ArticEventTxStarted const&) {
	service_active();
}

void ArgosTxService::react(ArticEventTxComplete const&) {
	m_sched.notify_tx_complete();
	service_complete();
}

void ArgosTxService::react(ArticEventDeviceError const&) {
	service_cancel();
	service_complete();
}

// ArgosPacketBuilder

unsigned int ArgosPacketBuilder::convert_speed(double x) {
	return (SECONDS_PER_HOUR * x) / (2*MM_PER_KM);
}

unsigned int ArgosPacketBuilder::convert_battery_voltage(unsigned int battery_voltage) {
	return std::min((unsigned int)127, (unsigned int)std::max((int)battery_voltage - (int)REF_BATT_MV, (int)0) / MV_PER_UNIT);
}

unsigned int ArgosPacketBuilder::convert_latitude(double x) {
	if (x >= 0)
		return x * LON_LAT_RESOLUTION;
	else
		return ((unsigned int)((x - 0.00005) * -LON_LAT_RESOLUTION)) | 1<<20; // -ve: bit 20 is sign
}

unsigned int ArgosPacketBuilder::convert_longitude(double x) {
	if (x >= 0)
		return x * LON_LAT_RESOLUTION;
	else
		return ((unsigned int)((x - 0.00005) * -LON_LAT_RESOLUTION)) | 1<<21; // -ve: bit 21 is sign
}

unsigned int ArgosPacketBuilder::convert_heading(double x) {
	return x * DEGREES_PER_UNIT;
}

unsigned int ArgosPacketBuilder::convert_altitude(double x) {
	return std::min((double)MAX_ALTITUDE, std::max((double)MIN_ALTITUDE, x / (MM_PER_METER * METRES_PER_UNIT)));
}

ArticPacket ArgosPacketBuilder::build_short_packet(GPSLogEntry* gps_entry,
		bool is_out_of_zone,
		bool is_low_battery
		) {

	DEBUG_TRACE("ArgosPacketBuilder::build_short_packet");
	unsigned int base_pos = 0;
	ArticPacket packet;

	// Reserve required number of bytes
	packet.assign(SHORT_PACKET_BYTES, 0);

	// Payload bytes
	PACK_BITS(0, packet, base_pos, 8);  // Zero CRC field (computed later)

	// Use scheduled GPS time as day/hour/min
	uint16_t year;
	uint8_t month, day, hour, min, sec;
	convert_datetime_to_epoch(gps_entry->info.schedTime, year, month, day, hour, min, sec);
	PACK_BITS(gps_entry->info.day, packet, base_pos, 5);

	DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: day=%u", (unsigned int)day);
	PACK_BITS(gps_entry->info.hour, packet, base_pos, 5);
	DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: hour=%u", (unsigned int)hour);
	PACK_BITS(gps_entry->info.min, packet, base_pos, 6);
	DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: min=%u", (unsigned int)min);

	if (gps_entry->info.valid) {
		unsigned int lat = convert_latitude(gps_entry->info.lat);
		PACK_BITS(lat, packet, base_pos, 21);
		DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: lat=%u (%lf)", lat, gps_entry->info.lat);
		unsigned int lon = convert_longitude(gps_entry->info.lon);
		PACK_BITS(lon, packet, base_pos, 22);
		DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: lon=%u (%lf)", lon, gps_entry->info.lon);
		unsigned int gspeed = convert_speed((double)gps_entry->info.gSpeed);
		PACK_BITS((unsigned int)gspeed, packet, base_pos, 7);
		DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: speed=%u", (unsigned int)gspeed);

		// OUTOFZONE_FLAG
		PACK_BITS(is_out_of_zone, packet, base_pos, 1);
		DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: is_out_of_zone=%u", is_out_of_zone);

		unsigned int heading = convert_heading(gps_entry->info.headMot);
		PACK_BITS(heading, packet, base_pos, 8);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: heading=%u", heading);
		if (gps_entry->info.fixType == FIXTYPE_3D) {
			unsigned int altitude = convert_altitude((double)gps_entry->info.hMSL);
			DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: altitude=%d (x 40m)", altitude);
			PACK_BITS(altitude, packet, base_pos, 8);
		} else {
			DEBUG_WARN("ArgosPacketBuilder::build_short_packet: altitude not available without 3D fix");
			PACK_BITS(INVALID_ALTITUDE, packet, base_pos, 8);
		}
	} else {
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 21);
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 22);
		PACK_BITS(0xFF, packet, base_pos, 7);
		PACK_BITS(is_out_of_zone, packet, base_pos, 1);
		DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: is_out_of_zone=%u", (unsigned int)is_out_of_zone);
		PACK_BITS(0xFF, packet, base_pos, 8);
		PACK_BITS(0xFF, packet, base_pos, 8);
	}

	unsigned int batt = convert_battery_voltage((unsigned int)gps_entry->info.batt_voltage);
	PACK_BITS(batt, packet, base_pos, 7);
	DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: voltage=%u (%u)", (unsigned int)batt, (unsigned int)gps_entry->info.batt_voltage);

	// LOWBATERY_FLAG
	PACK_BITS(is_low_battery, packet, base_pos, 1);
	DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: is_lb=%u", is_low_battery);

	// Calculate CRC8
	unsigned char crc8 = CRC8::checksum(packet.substr(1), SHORT_PACKET_PAYLOAD_BITS - 8);
	unsigned int crc_offset = 0;
	PACK_BITS(crc8, packet, crc_offset, 8);
	DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: crc8=%02x", crc8);

	// BCH code B127_106_3
	BCHCodeWord code_word = BCHEncoder::encode(
			BCHEncoder::B127_106_3,
			sizeof(BCHEncoder::B127_106_3),
			packet, SHORT_PACKET_PAYLOAD_BITS);
	DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: bch=%06x", code_word);

	// Append BCH code
	PACK_BITS(code_word, packet, base_pos, BCHEncoder::B127_106_3_CODE_LEN);

	return packet;
}

ArticPacket ArgosPacketBuilder::build_long_packet(std::vector<GPSLogEntry*> &gps_entries,
		bool is_out_of_zone,
		bool is_low_battery,
		BaseDeltaTimeLoc delta_time_loc) {
	unsigned int base_pos = 0;
	ArticPacket packet;

	DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: gps_entries: %u", gps_entries.size());

	// Reserve required number of bytes
	packet.assign(LONG_PACKET_BYTES, 0);

	// Payload bytes
	PACK_BITS(0, packet, base_pos, 8);  // Zero CRC field (computed later)

	// This will set the log time for the GPS entry based on when it was scheduled
	uint16_t year;
	uint8_t month, day, hour, min, sec;
	convert_datetime_to_epoch(gps_entries[0]->info.schedTime, year, month, day, hour, min, sec);

	PACK_BITS(day, packet, base_pos, 5);
	DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: day=%u", (unsigned int)day);
	PACK_BITS(hour, packet, base_pos, 5);
	DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: hour=%u", (unsigned int)hour);
	PACK_BITS(min, packet, base_pos, 6);
	DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: min=%u", (unsigned int)min);

	// First GPS entry
	if (gps_entries[0]->info.valid) {
		PACK_BITS(convert_latitude(gps_entries[0]->info.lat), packet, base_pos, 21);
		DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: lat=%u (%lf)", convert_latitude(gps_entries[0]->info.lat), gps_entries[0]->info.lat);
		PACK_BITS(convert_longitude(gps_entries[0]->info.lon), packet, base_pos, 22);
		DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: lon=%u (%lf)", convert_longitude(gps_entries[0]->info.lon), gps_entries[0]->info.lon);
		unsigned int gspeed = convert_speed(gps_entries[0]->info.gSpeed);
		PACK_BITS((unsigned int)gspeed, packet, base_pos, 7);
		DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: speed=%u", (unsigned int)gspeed);
	} else {
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 21);
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 22);
		PACK_BITS(0xFF, packet, base_pos, 7);
	}

	// OUTOFZONE_FLAG
	PACK_BITS(is_out_of_zone, packet, base_pos, 1);
	DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: is_out_of_zone=%u", is_out_of_zone);

	unsigned int batt = convert_battery_voltage(gps_entries[0]->info.batt_voltage);
	PACK_BITS(batt, packet, base_pos, 7);
	DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: voltage=%u (%u)", (unsigned int)batt, (unsigned int)gps_entries[0]->info.batt_voltage);

	// LOWBATERY_FLAG
	PACK_BITS(is_low_battery, packet, base_pos, 1);
	DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: is_lb=%u", (unsigned int)is_low_battery);

	// Delta time loc
	PACK_BITS((unsigned int)delta_time_loc, packet, base_pos, 4);
	DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: delta_time_loc=%u", (unsigned int)delta_time_loc);

	// Subsequent GPS entries
	for (unsigned int i = 1; i < MAX_GPS_ENTRIES_IN_PACKET; i++) {
		if (gps_entries.size() <= i || 0 == gps_entries[i]->info.valid) {
			DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: gps_entries[%u] not present/no fix");
			PACK_BITS(0xFFFFFFFF, packet, base_pos, 21);
			PACK_BITS(0xFFFFFFFF, packet, base_pos, 22);
		} else {
			PACK_BITS(convert_latitude(gps_entries[i]->info.lat), packet, base_pos, 21);
			DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: lat[%u]=%u (%lf)", i, convert_latitude(gps_entries[i]->info.lat), gps_entries[i]->info.lat);
			PACK_BITS(convert_longitude(gps_entries[i]->info.lon), packet, base_pos, 22);
			DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: lon[%u]=%u (%lf)", i, convert_longitude(gps_entries[i]->info.lon), gps_entries[i]->info.lon);
		}
	}

	// Calculate CRC8
	unsigned char crc8 = CRC8::checksum(packet.substr(1), LONG_PACKET_PAYLOAD_BITS - 8);
	unsigned int crc_offset = 0;
	PACK_BITS(crc8, packet, crc_offset, 8);
	DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: crc8=%02x", crc8);

	// BCH code B255_223_4
	BCHCodeWord code_word = BCHEncoder::encode(
			BCHEncoder::B255_223_4,
			sizeof(BCHEncoder::B255_223_4),
			packet, LONG_PACKET_PAYLOAD_BITS);
	DEBUG_TRACE("ArgosPacketBuilder::build_long_packet: bch=%08x", code_word);

	// Append BCH code
	PACK_BITS(code_word, packet, base_pos, BCHEncoder::B255_223_4_CODE_LEN);

	return packet;
}

ArticPacket ArgosPacketBuilder::build_gnss_packet(std::vector<GPSLogEntry*> &v,
		bool is_out_of_zone,
		bool is_low_battery,
		BaseDeltaTimeLoc delta_time_loc,
		unsigned int &size_bits) {
	if (v.size() > 1) {
		size_bits = LONG_PACKET_BITS;
		return build_long_packet(v, is_out_of_zone, is_low_battery, delta_time_loc);
	} else {
		size_bits = SHORT_PACKET_BITS;
		return build_short_packet(v[0], is_out_of_zone, is_low_battery);
	}
}

ArticPacket ArgosPacketBuilder::build_certification_packet(std::string cert_tx_payload, unsigned int &size_bits) {

	// Convert from ASCII hex to a real binary buffer
	ArticPacket packet = Binascii::unhexlify(cert_tx_payload);

	DEBUG_TRACE("ArgosPacketBuilder::build_certification_packet: TX payload size %u bytes", packet.size());

	// Check the size to determine the packet #bits to send in payload
	if (packet.size() > SHORT_PACKET_BYTES) {
		DEBUG_TRACE("ArgosPacketBuilder::build_certification_packet: using long packet");
		size_bits = LONG_PACKET_BITS;
		packet.resize(LONG_PACKET_BYTES);
	} else {
		DEBUG_TRACE("ArgosPacketBuilder::build_certification_packet: using short packet");
		size_bits = SHORT_PACKET_BITS;
		packet.resize(SHORT_PACKET_BYTES);
	}

	return packet;
}

ArticPacket ArgosPacketBuilder::build_doppler_packet(unsigned int batt_voltage, bool is_low_battery, unsigned int &size_bits) {
	DEBUG_TRACE("ArgosPacketBuilder::build_doppler_packet");
	unsigned int base_pos = 0;
	ArticPacket packet;

	// Reserve required number of bytes
	packet.assign(DOPPLER_PACKET_BYTES, 0);

	// Payload bytes
	PACK_BITS(0, packet, base_pos, 8);  // Zero CRC field (computed later)

	unsigned int last_known_pos = 0;
	PACK_BITS(last_known_pos, packet, base_pos, 8);
	DEBUG_TRACE("ArgosPacketBuilder::build_doppler_packet: last_known_pos=%u", (unsigned int)last_known_pos);

	unsigned int batt = convert_battery_voltage(batt_voltage);
	PACK_BITS(batt, packet, base_pos, 7);
	DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: voltage=%u (%u)", (unsigned int)batt, (unsigned int)batt_voltage);

	// LOWBATERY_FLAG
	PACK_BITS(is_low_battery, packet, base_pos, 1);
	DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: is_lb=%u", (unsigned int)is_low_battery);

	// Calculate CRC8
	unsigned char crc8 = CRC8::checksum(packet.substr(1), DOPPLER_PACKET_PAYLOAD_BITS - 8);
	unsigned int crc_offset = 0;
	PACK_BITS(crc8, packet, crc_offset, 8);
	DEBUG_TRACE("ArgosPacketBuilder::build_short_packet: crc8=%02x", crc8);

	size_bits = DOPPLER_PACKET_BITS;

	return packet;
}

// ArgosTxScheduler

ArgosTxScheduler::ArgosTxScheduler() : m_last_schedule_abs({}),
		 m_curr_schedule_abs({}),
		m_earliest_schedule({}),
		m_rand(std::mt19937()) {
	m_location.reset();
}

void ArgosTxScheduler::reset(unsigned int seed) {
	m_earliest_schedule = {};
	m_last_schedule_abs = {};
	m_location.reset();
	m_rand.seed(seed);
}

bool ArgosTxScheduler::is_in_duty_cycle(uint64_t time_ms, unsigned int duty_cycle)
{
	// Note that duty cycle is a bit-field comprising 24 bits as follows:
	// 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00  bit
	// 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 21 21 22 23  hour (UTC)
	uint64_t msec_of_day = (time_ms % (SECONDS_PER_DAY * MSECS_PER_SECOND));
	unsigned int hour_of_day = msec_of_day / (SECONDS_PER_HOUR * MSECS_PER_SECOND);
	return (duty_cycle & (0x800000 >> hour_of_day));
}

int ArgosTxScheduler::compute_random_jitter(bool jitter_en, int min, int max) {
	if (jitter_en) {
		std::uniform_int_distribution<int> dist(min, max);
		int jitter = dist(m_rand);
		DEBUG_TRACE("ArgosTxScheduler::compute_random_jitter: jitter=%d", jitter);
		return jitter;
	} else {
		return 0;
	}
}

void ArgosTxScheduler::schedule_periodic(unsigned int period_ms, bool jitter_en, unsigned int duty_cycle, uint64_t now_ms) {

	uint64_t start_time;

	DEBUG_TRACE("ArgosTxScheduler::schedule_periodic: now=%llu last=%llu tr=%u jitter=%u", now_ms,
			m_last_schedule_abs.has_value() ? m_last_schedule_abs.value() : 0,
			period_ms, jitter_en);

	// Handle the case where earliest TX time has been set
	while (m_earliest_schedule.has_value())
	{
		DEBUG_TRACE("ArgosScheduler::schedule_periodic: earliest TX is in %llu", m_earliest_schedule.value() - now_ms);

		// If earliest TX is later than last known schedule
		if (m_earliest_schedule.value() > now_ms) {
			// If we already had a schedule that is later then use that instead
			start_time = m_earliest_schedule.value();
			if (m_last_schedule_abs.has_value() &&
				start_time < m_last_schedule_abs.value())
				start_time = m_last_schedule_abs.value();

			// Check if start_time is in the duty cycle
			if (is_in_duty_cycle(start_time, duty_cycle)) {
				m_curr_schedule_abs = start_time;
				return;
			} else {
				// Not in duty cycle, so allow a new schedule to be computed
				break;
			}
		} else {
			// Earliest TX has elapsed so reset it
			m_earliest_schedule.reset();
		}
		break;
	}

	// A new schedule is required

	// Set schedule based on last TR_NOM point (if there is one)
	if (!m_last_schedule_abs.has_value()) {
		// Use now as the initial TR_NOM -- we don't allow
		// a -ve jitter amount in this case to avoid a potential -ve overflow
		start_time = now_ms + compute_random_jitter(jitter_en, 0);
	}
	else
	{
		// Advance by TR_NOM + TX jitter if we have a previous TR_NOM
		// It should be safe to allow a -ve jitter because TR_NOM is always larger
		// than the jitter amount
		start_time = m_last_schedule_abs.value() + period_ms + compute_random_jitter(jitter_en);
	}

	DEBUG_TRACE("ArgosTxScheduler::schedule_periodic: starting @ %llu", start_time);

	// We iterate forwards from the candidate start_time until we find a TR_NOM that
	// falls inside a permitted hour of transmission.  The maximum span we search is 24 hours.
	uint64_t elapsed_time = 0;
	while (elapsed_time <= (MSECS_PER_SECOND * SECONDS_PER_DAY)) {
		if (is_in_duty_cycle(start_time, duty_cycle) && start_time >= now_ms) {
			DEBUG_TRACE("ArgosTxScheduler::schedule_periodic: found schedule @ %llu", start_time);
			m_curr_schedule_abs = start_time;
			return;
		} else {
			start_time += period_ms;
			elapsed_time += period_ms;
		}
	}

	DEBUG_ERROR("ArgosTxScheduler::schedule_periodic: no schedule found!");
	m_curr_schedule_abs.reset();
	throw;
}

unsigned int ArgosTxScheduler::schedule_prepass(ArgosConfig& config, BasePassPredict& pass_predict, ArticMode& scheduled_mode, std::time_t now) {

	DEBUG_TRACE("ArgosTxScheduler::schedule_prepass");

	// We must have a previous GPS location to proceed
	if (!m_location.has_value()) {
		DEBUG_WARN("ArgosTxScheduler::schedule_prepass: current GPS location is not presently known - aborting");
		m_last_schedule_abs.reset();
		return INVALID_SCHEDULE;
	}

	// Assume start window position is current time
	uint64_t now_ms = now * MSECS_PER_SECOND;
	uint64_t start_time_ms = now_ms;

	// If we were deferred by UW event then recompute using a start window that reflects earliest TX
	if (m_earliest_schedule.has_value()) {
		if (m_earliest_schedule.value() > now_ms) {
			DEBUG_TRACE("ArgosScheduler::schedule_prepass: using earliest TX @ %.3f starting point", (double)(m_earliest_schedule.value() - now_ms) / MSECS_PER_SECOND);
			start_time_ms = m_earliest_schedule.value();
		} else {
			m_earliest_schedule.reset();
		}
	}

	// Set start and end time of the prepass search - we use a 24 hour window
	std::time_t start_time = start_time_ms / MSECS_PER_SECOND;
	std::time_t stop_time = start_time + (std::time_t)(24 * SECONDS_PER_HOUR);
	struct tm *p_tm = std::gmtime(&start_time);
	struct tm tm_start = *p_tm;
	p_tm = std::gmtime(&stop_time);
	struct tm tm_stop = *p_tm;

	DEBUG_INFO("ArgosTxScheduler::schedule_prepass: searching window start=%llu now=%llu stop=%llu", start_time, now, stop_time);

	PredictionPassConfiguration_t pp_config = {
		(float)m_location.value().latitude,
		(float)m_location.value().longitude,
		{ (uint16_t)(1900 + tm_start.tm_year), (uint8_t)(tm_start.tm_mon + 1), (uint8_t)tm_start.tm_mday, (uint8_t)tm_start.tm_hour, (uint8_t)tm_start.tm_min, (uint8_t)tm_start.tm_sec },
		{ (uint16_t)(1900 + tm_stop.tm_year), (uint8_t)(tm_stop.tm_mon + 1), (uint8_t)tm_stop.tm_mday, (uint8_t)tm_stop.tm_hour, (uint8_t)tm_stop.tm_min, (uint8_t)tm_stop.tm_sec },
        (float)config.prepass_min_elevation,        //< Minimum elevation of passes [0, 90]
		(float)config.prepass_max_elevation,        //< Maximum elevation of passes  [maxElevation >= < minElevation]
		(float)config.prepass_min_duration / 60.0f,  //< Minimum duration (minutes)
		config.prepass_max_passes,                  //< Maximum number of passes per satellite (#)
		(float)config.prepass_linear_margin / 60.0f, //< Linear time margin (in minutes/6months)
		config.prepass_comp_step                    //< Computation step (seconds)
	};
	SatelliteNextPassPrediction_t next_pass;

	while (PREVIPASS_compute_next_pass(
    		&pp_config,
			pass_predict.records,
			pass_predict.num_records,
			&next_pass)) {

		// No schedule
		uint64_t schedule = 0;

		// If there is a previous transmission then make sure schedule is at least advance TR_NOM
		if (m_last_schedule_abs.has_value())
			schedule = std::max((uint64_t)schedule, m_last_schedule_abs.value() + (config.tr_nom * MSECS_PER_SECOND));

		// Advance to at least the prepass epoch position
		schedule = std::max(((uint64_t)next_pass.epoch * MSECS_PER_SECOND), schedule);

		// Apply nominal jitter to schedule
		// NOTE: Because of the possibility of -ve jitter resulting in an edge case we have to make
		// sure the schedule is advanced passed at least start_time and curr_time
		schedule += compute_random_jitter(config.argos_tx_jitter_en);

		// Make sure computed schedule is at least start_time_ms
		schedule = std::max(start_time_ms, schedule);

		// Make sure computed schedule is at least current RTC time to avoid a -ve schedule
		schedule = std::max(now_ms, schedule);

		DEBUG_INFO("ArgosTxScheduler::schedule_prepass: hex_id=%01x dl=%u ul=%u last=%.3f s=%.3f c=%.3f e=%.3f",
					(unsigned int)next_pass.satHexId,
					(unsigned int)next_pass.downlinkStatus,
					(unsigned int)next_pass.uplinkStatus,
					!m_last_schedule_abs.has_value() ? 0 :
							((double)(m_last_schedule_abs.value() / MSECS_PER_SECOND) - now), (double)next_pass.epoch - (double)now,
							((double)schedule / MSECS_PER_SECOND) - now, ((double)next_pass.epoch + (double)next_pass.duration) - now);

		// Check we don't transmit off the end of the prepass window
		if ((schedule + ARGOS_TX_MARGIN_MSECS) < ((uint64_t)next_pass.epoch + next_pass.duration) * MSECS_PER_SECOND) {
			// We're good to go for this schedule, compute relative delay until the epoch arrives
			// and set the required Argos transmission mode
			DEBUG_INFO("ArgosTxScheduler::schedule_prepass: scheduled for %.3f seconds from now", (double)(schedule - (now * MSECS_PER_SECOND)) / MSECS_PER_SECOND);
			m_curr_schedule_abs = schedule;
			scheduled_mode = next_pass.uplinkStatus >= SAT_UPLK_ON_WITH_A3 ? ArticMode::A3 : ArticMode::A2;
			return m_curr_schedule_abs.value() - now_ms;
		} else {
			DEBUG_TRACE("ArgosTxScheduler::schedule_prepass: computed schedule is too late for this window", next_pass.epoch, next_pass.duration);
			start_time = (std::time_t)next_pass.epoch + next_pass.duration;
			p_tm = std::gmtime(&start_time);
			tm_start = *p_tm;
			pp_config.start = { (uint16_t)(1900 + tm_start.tm_year), (uint8_t)(tm_start.tm_mon + 1), (uint8_t)tm_start.tm_mday, (uint8_t)tm_start.tm_hour, (uint8_t)tm_start.tm_min, (uint8_t)tm_start.tm_sec };
			DEBUG_INFO("ArgosTxScheduler::schedule_prepass: searching window start=%llu now=%llu stop=%llu", start_time, now, stop_time);
		}
	}

	// No passes reported
	DEBUG_ERROR("ArgosTxScheduler::schedule_prepass: PREVIPASS_compute_next_pass returned no passes");
	return INVALID_SCHEDULE;
}

unsigned int ArgosTxScheduler::schedule_duty_cycle(ArgosConfig& config, std::time_t now) {
	try {
		schedule_periodic((config.tr_nom * MSECS_PER_SECOND), config.argos_tx_jitter_en, config.duty_cycle, ((uint64_t)now * MSECS_PER_SECOND));
		return m_curr_schedule_abs.value() - (now * MSECS_PER_SECOND);
	} catch(...) {
		return INVALID_SCHEDULE;
	}
}

unsigned int ArgosTxScheduler::schedule_legacy(ArgosConfig& config, std::time_t now) {
	try {
		schedule_periodic((config.tr_nom * MSECS_PER_SECOND), config.argos_tx_jitter_en, DUTYCYCLE_24HRS, (now * MSECS_PER_SECOND));
		return m_curr_schedule_abs.value() - (now * MSECS_PER_SECOND);
	} catch(...) {
		return INVALID_SCHEDULE;
	}
}

void ArgosTxScheduler::set_earliest_schedule(std::time_t earliest) {
	DEBUG_TRACE("ArgosTxScheduler::set_earliest_schedule: t=%llu", earliest);
	m_earliest_schedule = (uint64_t)earliest * MSECS_PER_SECOND;
}

void ArgosTxScheduler::set_last_location(double lon, double lat) {
	m_location = Location(lon, lat);
}

void ArgosTxScheduler::schedule_at(std::time_t t) {
	DEBUG_TRACE("ArgosTxScheduler::schedule_at: t=%llu", t);
	m_curr_schedule_abs = (uint64_t)t * MSECS_PER_SECOND;
}

void ArgosTxScheduler::notify_tx_complete() {
	m_last_schedule_abs = m_curr_schedule_abs;
}
