
#include <algorithm>
#include <climits>
#include <iomanip>
#include <ctime>

#include "messages.hpp"
#include "argos_scheduler.hpp"
#include "rtc.hpp"
#include "config_store.hpp"
#include "scheduler.hpp"
#include "bitpack.hpp"
#include "timeutils.hpp"
#include "bch.hpp"
#include "crc8.hpp"

extern "C" {
	#include "previpass.h"
}

#define INVALID_SCHEDULE    (std::time_t)-1

#define FIXTYPE_3D			3

#define HOURS_PER_DAY       24
#define SECONDS_PER_MINUTE	60
#define SECONDS_PER_HOUR    3600
#define SECONDS_PER_DAY     (SECONDS_PER_HOUR * HOURS_PER_DAY)
#define MM_PER_METER		1000
#define MM_PER_KM   		1000000
#define MV_PER_UNIT			30
#define MS_PER_SEC			1000
#define METRES_PER_UNIT     40
#define DEGREES_PER_UNIT	(1.0f/1.42f)
#define BITS_PER_BYTE		8
#define MIN_ALTITUDE		0
#define MAX_ALTITUDE		254
#define INVALID_ALTITUDE	255

#define LON_LAT_RESOLUTION  10000

#define MAX_GPS_ENTRIES_IN_PACKET	4

#define SHORT_PACKET_HEADER_BYTES   7
#define SHORT_PACKET_PAYLOAD_BITS   99
#define SHORT_PACKET_BYTES			22
#define SHORT_PACKET_MSG_LENGTH		6
#define SHORT_PACKET_BITFIELD       0x11

#define LONG_PACKET_HEADER_BYTES    7
#define LONG_PACKET_PAYLOAD_BITS    216
#define LONG_PACKET_BYTES			38
#define LONG_PACKET_MSG_LENGTH		15
#define LONG_PACKET_BITFIELD        0x8B

#define PACKET_SYNC					0xFFFE2F

#define ARGOS_TX_MARGIN_SECS        12

extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;
extern RTC       *rtc;
extern Logger    *sensor_log;


ArgosScheduler::ArgosScheduler() {
	m_is_running = false;
	m_switch_state = false;
	m_earliest_tx = INVALID_SCHEDULE;
	m_last_longitude = INVALID_GEODESIC;
	m_last_latitude = INVALID_GEODESIC;
}

void ArgosScheduler::reschedule() {
	std::time_t schedule = INVALID_SCHEDULE;

	// Obtain fresh copy of configuration as it may have changed
	configuration_store->get_argos_configuration(m_argos_config);

	if (m_argos_config.mode == BaseArgosMode::OFF) {
		DEBUG_WARN("ArgosScheduler: mode is OFF -- not scheduling");
		return;
	} else if (!rtc->is_set()) {
		DEBUG_WARN("ArgosScheduler: RTC is not yet set -- not scheduling");
		return;
	} else if (m_argos_config.mode == BaseArgosMode::LEGACY) {
		// In legacy mode we schedule every hour aligned to UTC
		schedule = next_duty_cycle(0xFFFFFFU);
	} else if (m_argos_config.mode == BaseArgosMode::DUTY_CYCLE) {
		schedule = next_duty_cycle(m_argos_config.duty_cycle);
	} else if (m_argos_config.mode == BaseArgosMode::PASS_PREDICTION) {
		schedule = next_prepass();
	}

	if (INVALID_SCHEDULE != schedule) {
		DEBUG_INFO("ArgosScheduler: schedule in: %llu secs", schedule);
		deschedule();
		m_argos_task = system_scheduler->post_task_prio(std::bind(&ArgosScheduler::process_schedule, this),
				"ArgosSchedulerProcessSchedule",
				Scheduler::DEFAULT_PRIORITY, MS_PER_SEC * schedule);
	} else {
		DEBUG_INFO("ArgosScheduler: not rescheduling");
	}
}

static inline bool is_in_duty_cycle(std::time_t time, unsigned int duty_cycle)
{
	unsigned int seconds_of_day = (time % SECONDS_PER_DAY);
	unsigned int hour_of_day = seconds_of_day / SECONDS_PER_HOUR;
	return (duty_cycle & (0x800000 >> hour_of_day));
}

std::time_t ArgosScheduler::next_duty_cycle(unsigned int duty_cycle)
{
	// Do not proceed unless duty cycle is non-zero to save time
	if (duty_cycle == 0)
	{
		DEBUG_INFO("ArgosScheduler::next_duty_cycle: no schedule found as duty cycle is zero!");
		m_tr_nom_schedule = INVALID_SCHEDULE;
		return INVALID_SCHEDULE;
	}

	// Find epoch time for start of the "current" day
	std::time_t now = rtc->gettime();
	unsigned int start_of_day = now - (now % SECONDS_PER_DAY);

	// If we have already a future schedule then don't recompute
	if (m_tr_nom_schedule != INVALID_SCHEDULE && m_tr_nom_schedule >= now &&
		m_tr_nom_schedule > m_last_transmission_schedule)
	{
		if (m_is_deferred) {
			DEBUG_INFO("ArgosScheduler::next_duty_cycle: existing schedule deferred: in %llu seconds", m_tr_nom_schedule - now);
			m_is_deferred = false;
			return m_tr_nom_schedule - now;
		}

		DEBUG_INFO("ArgosScheduler::next_duty_cycle: use existing schedule: in %llu seconds", m_tr_nom_schedule - now);
		return INVALID_SCHEDULE;
	}

	// If earliest TX is inside the duty cycle window, then use that since we were deferred by saltwater switch
	if (m_is_deferred && m_earliest_tx != INVALID_SCHEDULE && m_earliest_tx > now && is_in_duty_cycle(m_earliest_tx, duty_cycle))
	{
		DEBUG_INFO("ArgosScheduler::next_duty_cycle: using earliest TX: %llu", m_earliest_tx);
		m_is_deferred = false;
		return m_earliest_tx - now;
	}

	// Set schedule to be earliest possible next TR_NOM.  If there was no last schedule or
	// the last schedule was over 24 hours ago, then set our starting point to current time.
	// Otherwise increment the last schedule by TR_NOM for our starting point.
	if (m_tr_nom_schedule == INVALID_SCHEDULE || (now - m_tr_nom_schedule) > HOURS_PER_DAY)
		m_tr_nom_schedule = start_of_day;
	else
		m_tr_nom_schedule += m_argos_config.tr_nom;

	// Compute the seconds of day and hours of day for candidate m_tr_nom_schedule
	unsigned int seconds_of_day = (m_tr_nom_schedule % SECONDS_PER_DAY);
	unsigned int hour_of_day = (seconds_of_day / SECONDS_PER_HOUR);
	unsigned int terminal_hours = hour_of_day + (2*HOURS_PER_DAY);

	// Note that duty cycle is a bit-field comprising 24 bits as follows:
	// 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00  bit
	// 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 21 21 22 23  hour (UTC)
	// The range of TR_NOM is 45 seconds to 20 hours
	//
	// We iterate forwards from the candidate m_tr_nom_schedule until we find a TR_NOM that
	// falls inside a permitted hour of transmission.  The maximum span we search is 24 hours.
	while (hour_of_day < terminal_hours) {
		//DEBUG_TRACE("ArgosScheduler::next_duty_cycle: now: %lu candidate schedule: %lu hour_of_day: %u", now, m_tr_nom_schedule, hour_of_day);
		if ((duty_cycle & (0x800000 >> (hour_of_day % HOURS_PER_DAY))) && m_tr_nom_schedule >= now) {
			DEBUG_INFO("ArgosScheduler::next_duty_cycle: found schedule: %lu", m_tr_nom_schedule);
			return m_tr_nom_schedule - now;
		} else {
			m_tr_nom_schedule += m_argos_config.tr_nom;
			seconds_of_day += m_argos_config.tr_nom;
			hour_of_day = (seconds_of_day / SECONDS_PER_HOUR);
		}
	}

	DEBUG_WARN("ArgosScheduler::next_duty_cycle: no schedule found!");

	m_tr_nom_schedule = INVALID_SCHEDULE;

	return INVALID_SCHEDULE;
}

std::time_t ArgosScheduler::next_prepass() {

	DEBUG_TRACE("ArgosScheduler::next_prepass");

	// We must have a previous GPS location to proceed
	if (m_last_latitude == INVALID_GEODESIC) {
		DEBUG_WARN("ArgosScheduler::next_prepass: current GPS location is not presently known - aborting");
		return INVALID_SCHEDULE;
	}

	std::time_t curr_time = rtc->gettime();

	// If we have an existing prepass schedule and we were interrupted by saltwater switch, then check
	// to see if the current prepass schedule is still valid to use.  We allow up to ARGOS_TX_MARGIN_SECS
	// before the end of the window
	if (m_next_prepass != INVALID_SCHEDULE &&
		m_is_deferred &&
		m_earliest_tx != INVALID_SCHEDULE &&
		m_earliest_tx > curr_time &&
		m_earliest_tx < (m_next_prepass + m_prepass_duration - ARGOS_TX_MARGIN_SECS))
	{
		DEBUG_INFO("ArgosScheduler::next_prepass: using earliest TX=%lu", m_earliest_tx);
		m_is_deferred = false;
		return m_earliest_tx - curr_time;
	}

	if (m_next_prepass != INVALID_SCHEDULE &&
		curr_time < (m_next_prepass + m_prepass_duration - ARGOS_TX_MARGIN_SECS)) {
		DEBUG_WARN("ArgosScheduler::next_prepass: existing prepass schedule for epoch=%llu secs from now is still valid", m_next_prepass - curr_time);
		return INVALID_SCHEDULE;
	}

	// Ensure start window is sufficiently advanced from previous prepass window to avoid repeated transmissions
	// in the existing window
	std::time_t start_time;
	if (m_last_transmission_schedule != INVALID_SCHEDULE)
		start_time = std::max(curr_time, (m_last_transmission_schedule + m_prepass_duration + 1));
	else
		start_time = curr_time;

	std::time_t stop_time = start_time + (std::time_t)(24 * SECONDS_PER_HOUR);
	struct tm *p_tm = std::gmtime(&start_time);
	struct tm tm_start = *p_tm;
	p_tm = std::gmtime(&stop_time);
	struct tm tm_stop = *p_tm;

	DEBUG_INFO("ArgosScheduler::next_prepass: now=%llu start=%llu stop=%llu", curr_time, start_time, stop_time);

	BasePassPredict& pass_predict = configuration_store->read_pass_predict();
	PredictionPassConfiguration_t config = {
		(float)m_last_latitude,
		(float)m_last_longitude,
		{ (uint16_t)(1900 + tm_start.tm_year), (uint8_t)(tm_start.tm_mon + 1), (uint8_t)tm_start.tm_mday, (uint8_t)tm_start.tm_hour, (uint8_t)tm_start.tm_min, (uint8_t)tm_start.tm_sec },
		{ (uint16_t)(1900 + tm_stop.tm_year), (uint8_t)(tm_stop.tm_mon + 1), (uint8_t)tm_stop.tm_mday, (uint8_t)tm_stop.tm_hour, (uint8_t)tm_stop.tm_min, (uint8_t)tm_stop.tm_sec },
        (float)m_argos_config.prepass_min_elevation,        //< Minimum elevation of passes [0, 90]
		(float)m_argos_config.prepass_max_elevation,        //< Maximum elevation of passes  [maxElevation >= < minElevation]
		(float)m_argos_config.prepass_min_duration / 60.0f,  //< Minimum duration (minutes)
		m_argos_config.prepass_max_passes,                  //< Maximum number of passes per satellite (#)
		(float)m_argos_config.prepass_linear_margin / 60.0f, //< Linear time margin (in minutes/6months)
		m_argos_config.prepass_comp_step                    //< Computation step (seconds)
	};
	SatelliteNextPassPrediction_t next_pass;

	if (PREVIPASS_compute_next_pass(
    		&config,
			pass_predict.records,
			pass_predict.num_records,
			&next_pass)) {

		DEBUG_INFO("ArgosScheduler::next_prepass: hex_id=%01x now=%llu epoch=%u duration=%u elevation_max=%u ul_status=%01x",
					next_pass.satHexId, curr_time, (unsigned int)next_pass.epoch, (unsigned int)next_pass.duration, (unsigned int)next_pass.elevationMax, (unsigned char)next_pass.uplinkStatus);

		std::time_t now = rtc->gettime();
		if (now <= ((std::time_t)next_pass.epoch + next_pass.duration - ARGOS_TX_MARGIN_SECS)) {
			// Compute delay until epoch arrives for scheduling and select Argos transmission mode
			std::time_t schedule = (std::time_t)next_pass.epoch < now ? now : (std::time_t)next_pass.epoch;
			DEBUG_INFO("ArgosScheduler::next_prepass: scheduled for %llu seconds from now", schedule - now);
			m_next_prepass = schedule;
			m_next_mode = next_pass.uplinkStatus >= SAT_UPLK_ON_WITH_A3 ? ArgosMode::ARGOS_3 : ArgosMode::ARGOS_2;
			m_prepass_duration = next_pass.duration;
			return schedule - now;
		} else {
			DEBUG_WARN("ArgosScheduler::next_prepass: computed prepass window=[%llu,%u] is too late", next_pass.epoch, next_pass.duration);
			return INVALID_SCHEDULE;
		}
	} else {
		// No passes reported
		DEBUG_WARN("ArgosScheduler::next_prepass: PREVIPASS_compute_next_pass returned no passes");
		return INVALID_SCHEDULE;
	}
}

void ArgosScheduler::process_schedule() {

	std::time_t now = rtc->gettime();
	if (!m_switch_state && (m_earliest_tx == INVALID_SCHEDULE || m_earliest_tx <= now)) {
		if (m_argos_config.mode == BaseArgosMode::PASS_PREDICTION) {
			pass_prediction_algorithm();
		} else {
			periodic_algorithm();
		}
	} else {
		DEBUG_INFO("ArgosScheduler::process_schedule: sws=%u t=%llu earliest_tx=%llu deferring transmission", m_switch_state, now, m_earliest_tx);
		m_is_deferred = true;
	}

	// After each transmission attempt to reschedule
	reschedule();
}

void ArgosScheduler::pass_prediction_algorithm() {
	unsigned int max_index = (((unsigned int)m_argos_config.depth_pile + MAX_GPS_ENTRIES_IN_PACKET-1) / MAX_GPS_ENTRIES_IN_PACKET);
	unsigned int index = 0;
	unsigned int max_msg_index;
	ArgosPacket packet;

	DEBUG_INFO("ArgosScheduler::pass_prediction_algorithm: m_msg_index=%u max_index=%u", m_msg_index, max_index);

	// Find first eligible slot for transmission
	max_msg_index = m_msg_index + max_index;
	while (m_msg_index < max_msg_index) {
		index = m_msg_index % max_index;
		//DEBUG_TRACE("ArgosScheduler::pass_prediction_algorithm: m_msg_index=%u index=%u gps_entries=%u", m_msg_index, index, m_gps_entries[index].size());
		if (m_gps_entries[index].size())
		{
			DEBUG_TRACE("ArgosScheduler::pass_prediction_algorithm: eligible slot=%u", index);
			break;
		}
		m_msg_index++;
	}

	// No further action if no slot is eligible for transmission
	if (m_msg_index == max_msg_index) {
		DEBUG_WARN("ArgosScheduler::pass_prediction_algorithm: no eligible slot found");
		m_last_transmission_schedule = m_next_prepass;
		m_next_prepass = INVALID_SCHEDULE;
		return;
	}

	if (m_gps_entries[index].size() == 1) {
		build_short_packet(m_gps_entries[index][0], packet);
		handle_packet(packet, SHORT_PACKET_BYTES * BITS_PER_BYTE, m_next_mode);
	} else {
		build_long_packet(m_gps_entries[index], packet);
		handle_packet(packet, LONG_PACKET_BYTES * BITS_PER_BYTE, m_next_mode);
	}

	m_msg_burst_counter[index]--;
	m_msg_index++;
	m_last_transmission_schedule = m_next_prepass;
	m_next_prepass = INVALID_SCHEDULE;

	// Check if message burst count has reached zero and perform clean-up of this slot
	if (m_msg_burst_counter[index] == 0) {
		m_msg_burst_counter[index] = (m_argos_config.ntry_per_message == 0) ? UINT_MAX : m_argos_config.ntry_per_message;
		m_gps_entries[index].clear();
	}
}

void ArgosScheduler::notify_sensor_log_update() {
	DEBUG_TRACE("ArgosScheduler::notify_sensor_log_update");

	if (m_is_running) {
		GPSLogEntry gps_entry;

		// Read the most recent GPS entry out of the sensor log
		unsigned int idx = sensor_log->num_entries() - 1;  // Most recent entry in log
		sensor_log->read(&gps_entry, idx);
		// Update last known position if the GPS entry is valid (otherwise we preserve the last one)
		if (gps_entry.info.valid) {
			DEBUG_TRACE("ArgosScheduler::notify_sensor_log_update: updated last known GPS position; is_rtc_set=%u",
					rtc->is_set());
			m_last_longitude = gps_entry.info.lon;
			m_last_latitude = gps_entry.info.lat;
		}

		if (m_argos_config.mode == BaseArgosMode::PASS_PREDICTION) {
			unsigned int msg_index;
			unsigned int max_index = (((unsigned int)m_argos_config.depth_pile + MAX_GPS_ENTRIES_IN_PACKET-1) / MAX_GPS_ENTRIES_IN_PACKET);
			unsigned int span = std::min((unsigned int)MAX_GPS_ENTRIES_IN_PACKET, (unsigned int)m_argos_config.depth_pile);

			DEBUG_TRACE("ArgosScheduler::notify_sensor_log_update: PASS_PREDICTION mode: max_index=%u span=%u", max_index, span);

			// Find the first slot whose vector has less then depth pile entries in it
			for (msg_index = 0; msg_index < max_index; msg_index++) {
				if (m_gps_entries[msg_index].size() < span)
					break;
			}

			// If a slot is found then append most recent sensor log entry to it
			if (msg_index < max_index) {
				m_gps_entries[msg_index].push_back(gps_entry);
				DEBUG_TRACE("ArgosScheduler::notify_sensor_log_update: PASS_PREDICTION mode: appending entry=%u to msg_slot=%u", idx, msg_index);
			} else {
				DEBUG_TRACE("ArgosScheduler::notify_sensor_log_update: PASS_PREDICTION mode: no free slots");
			}
		} else {
			// Reset the message burst counters
			for (unsigned int i = 0; i < MAX_MSG_INDEX; i++) {
				m_msg_burst_counter[i] = (m_argos_config.ntry_per_message == 0) ? UINT_MAX : m_argos_config.ntry_per_message;
			}
		}
		reschedule();
	}
}

unsigned int ArgosScheduler::convert_latitude(double x) {
	if (x >= 0)
		return x * LON_LAT_RESOLUTION;
	else
		return ((unsigned int)((x - 0.00005) * -LON_LAT_RESOLUTION)) | 1<<20; // -ve: bit 20 is sign
}

unsigned int ArgosScheduler::convert_longitude(double x) {
	if (x >= 0)
		return x * LON_LAT_RESOLUTION;
	else
		return ((unsigned int)((x - 0.00005) * -LON_LAT_RESOLUTION)) | 1<<21; // -ve: bit 21 is sign
}

void ArgosScheduler::build_short_packet(GPSLogEntry const& gps_entry, ArgosPacket& packet) {

	DEBUG_TRACE("ArgosScheduler::build_short_packet");

#ifndef ARGOS_TEST_PACKET
	unsigned int base_pos = 0;

	// Reserve required number of bytes
	packet.assign(SHORT_PACKET_BYTES, 0);

	// Header bytes
	PACK_BITS(PACKET_SYNC, packet, base_pos, 24);
	PACK_BITS(SHORT_PACKET_MSG_LENGTH, packet, base_pos, 4);
	PACK_BITS(m_argos_config.argos_id>>8, packet, base_pos, 20);
	PACK_BITS(m_argos_config.argos_id, packet, base_pos, 8);

	// Payload bytes
#ifdef ARGOS_USE_CRC8
	PACK_BITS(0, packet, base_pos, 8);  // Zero CRC field (computed later)
#else
	PACK_BITS(SHORT_PACKET_BITFIELD, packet, base_pos, 8);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: bitfield=%u", SHORT_PACKET_BITFIELD);
#endif
	PACK_BITS(gps_entry.info.day, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: day=%u", gps_entry.info.day);
	PACK_BITS(gps_entry.info.hour, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: hour=%u", gps_entry.info.hour);
	PACK_BITS(gps_entry.info.min, packet, base_pos, 6);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: min=%u", gps_entry.info.min);

	if (gps_entry.info.valid) {
		PACK_BITS(convert_latitude(gps_entry.info.lat), packet, base_pos, 21);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: lat=%u (%lf)", convert_latitude(gps_entry.info.lat), gps_entry.info.lat);
		PACK_BITS(convert_longitude(gps_entry.info.lon), packet, base_pos, 22);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: lon=%u (%lf)", convert_longitude(gps_entry.info.lon), gps_entry.info.lon);
		double gspeed = (SECONDS_PER_HOUR * gps_entry.info.gSpeed) / MM_PER_KM;
		PACK_BITS((unsigned int)gspeed, packet, base_pos, 8);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: speed=%u", (unsigned int)gspeed);
		PACK_BITS(gps_entry.info.headMot * DEGREES_PER_UNIT, packet, base_pos, 8);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: heading=%u", (unsigned int)(gps_entry.info.headMot * DEGREES_PER_UNIT));
		if (gps_entry.info.fixType == FIXTYPE_3D) {
			int32_t altitude = gps_entry.info.hMSL / (MM_PER_METER * METRES_PER_UNIT);
			if (altitude > MAX_ALTITUDE) {
				DEBUG_WARN("ArgosScheduler::build_short_packet: altitude %d (x 40m) exceeds maximum - truncating", altitude);
				altitude = MAX_ALTITUDE;
			} else if (altitude < MIN_ALTITUDE) {
				DEBUG_WARN("ArgosScheduler::build_short_packet: altitude %d (x 40m) below minimum - truncating", altitude);
				altitude = MIN_ALTITUDE;
			}
			DEBUG_TRACE("ArgosScheduler::build_short_packet: altitude=%d (x 40m)", altitude);
			PACK_BITS(altitude, packet, base_pos, 8);
		} else {
			DEBUG_WARN("ArgosScheduler::build_short_packet: altitude not available without 3D fix");
			PACK_BITS(INVALID_ALTITUDE, packet, base_pos, 8);
		}
	} else {
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 21);
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 22);
		PACK_BITS(0xFF, packet, base_pos, 8);
		PACK_BITS(0xFF, packet, base_pos, 8);
		PACK_BITS(0xFF, packet, base_pos, 8);
	}

	PACK_BITS(gps_entry.info.batt_voltage / MV_PER_UNIT, packet, base_pos, 8);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: voltage=%u", gps_entry.info.batt_voltage / MV_PER_UNIT);

	// Calculate CRC8
#ifdef ARGOS_USE_CRC8
	unsigned char crc8 = CRC8::checksum(packet.substr(SHORT_PACKET_HEADER_BYTES+1), SHORT_PACKET_PAYLOAD_BITS - 8);
	unsigned int crc_offset = 8*SHORT_PACKET_HEADER_BYTES;
	PACK_BITS(crc8, packet, crc_offset, 8);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: crc8=%02x", crc8);
#endif

	// BCH code B127_106_3
	BCHCodeWord code_word = BCHEncoder::encode(
			BCHEncoder::B127_106_3,
			sizeof(BCHEncoder::B127_106_3),
			packet.substr(SHORT_PACKET_HEADER_BYTES), SHORT_PACKET_PAYLOAD_BITS);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: bch=%06x", code_word);

	// Append BCH code
	PACK_BITS(code_word, packet, base_pos, BCHEncoder::B127_106_3_CODE_LEN);
#else

	// Send a nail-up test packet
	packet = std::string("\xFF\xFF\xFF\x64\xE7\xB5\x6A\xC1\x47\xCA\x6B\x48\x17\xC7\x65\xDC\x8A\x2A\x9D\xA1\xE2\x18"s);

#endif
}

BaseDeltaTimeLoc ArgosScheduler::delta_time_loc(GPSLogEntry const& a, GPSLogEntry const& b)
{
	std::time_t t_a, t_b, t_diff;

	t_a = convert_epochtime(a.info.year, a.info.month, a.info.day, a.info.hour, a.info.min, a.info.sec);
	t_b = convert_epochtime(b.info.year, b.info.month, b.info.day, b.info.hour, b.info.min, b.info.sec);

	if (t_a >= t_b) {
		t_diff = t_a - t_b;
	} else {
		t_diff = t_b - t_a;
	}

	if (t_diff >= (24 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_24HR;
	} else if (t_diff >= (12 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_12HR;
	} else if (t_diff >= (6 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_6HR;
	} else if (t_diff >= (4 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_4HR;
	} else if (t_diff >= (3 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_3HR;
	} else if (t_diff >= (2 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_2HR;
	} else if (t_diff >= (1 * SECONDS_PER_HOUR)) {
		return BaseDeltaTimeLoc::DELTA_T_1HR;
	} else if (t_diff >= (30 * SECONDS_PER_MINUTE)) {
		return BaseDeltaTimeLoc::DELTA_T_30MIN;
	} else if (t_diff >= (15 * SECONDS_PER_MINUTE)) {
		return BaseDeltaTimeLoc::DELTA_T_15MIN;
	} else {
		return BaseDeltaTimeLoc::DELTA_T_10MIN;
	}
}

void ArgosScheduler::build_long_packet(std::vector<GPSLogEntry> const& gps_entries, ArgosPacket& packet)
{
	unsigned int base_pos = 0;

	DEBUG_TRACE("ArgosScheduler::build_long_packet: gps_entries: %u", gps_entries.size());

	// Must be at least two GPS entries in the vector to use long packet format
	if (gps_entries.size() < 2) {
		DEBUG_ERROR("ArgosScheduler::build_long_packet: requires at least 2 GPS entries in vector (only %u)", gps_entries.size());
		return;
	}

	// Reserve required number of bytes
	packet.assign(LONG_PACKET_BYTES, 0);

	// Header bytes
	PACK_BITS(PACKET_SYNC, packet, base_pos, 24);
	PACK_BITS(LONG_PACKET_MSG_LENGTH, packet, base_pos, 4);
	PACK_BITS(m_argos_config.argos_id>>8, packet, base_pos, 20);
	PACK_BITS(m_argos_config.argos_id, packet, base_pos, 8);

	// Payload bytes
#ifdef ARGOS_USE_CRC8
	PACK_BITS(0, packet, base_pos, 8);  // Zero CRC field (computed later)
#else
	PACK_BITS(LONG_PACKET_BITFIELD, packet, base_pos, 8);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: bitfield=%u", LONG_PACKET_BITFIELD);
#endif
	PACK_BITS(gps_entries[0].info.day, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: day=%u", gps_entries[0].info.day);
	PACK_BITS(gps_entries[0].info.hour, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: hour=%u", gps_entries[0].info.hour);
	PACK_BITS(gps_entries[0].info.min, packet, base_pos, 6);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: min=%u", gps_entries[0].info.min);

	// First GPS entry
	if (gps_entries[0].info.valid) {
		PACK_BITS(convert_latitude(gps_entries[0].info.lat), packet, base_pos, 21);
		DEBUG_TRACE("ArgosScheduler::build_long_packet: lat=%u (%lf)", convert_latitude(gps_entries[0].info.lat), gps_entries[0].info.lat);
		PACK_BITS(convert_longitude(gps_entries[0].info.lon), packet, base_pos, 22);
		DEBUG_TRACE("ArgosScheduler::build_long_packet: lon=%u (%lf)", convert_longitude(gps_entries[0].info.lon), gps_entries[0].info.lon);
		double gspeed = (SECONDS_PER_HOUR * gps_entries[0].info.gSpeed) / MM_PER_KM;
		PACK_BITS((unsigned int)gspeed, packet, base_pos, 8);
		DEBUG_TRACE("ArgosScheduler::build_long_packet: speed=%u", (unsigned int)gspeed);
	} else {
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 21);
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 22);
		PACK_BITS(0xFF, packet, base_pos, 8);
	}

	PACK_BITS(gps_entries[0].info.batt_voltage / MV_PER_UNIT, packet, base_pos, 8);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: voltage=%u", gps_entries[0].info.batt_voltage / MV_PER_UNIT);

	// Delta time loc
	PACK_BITS((unsigned int)delta_time_loc(gps_entries[0], gps_entries[1]), packet, base_pos, 4);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: delta_time_loc=%u", (unsigned int)delta_time_loc(gps_entries[0], gps_entries[1]));

	// Subsequent GPS entries
	for (unsigned int i = 1; i < MAX_GPS_ENTRIES_IN_PACKET; i++) {
		DEBUG_TRACE("ArgosScheduler::build_long_packet: gps_valid[%u]=%u", i, gps_entries[i].info.valid);
		if (gps_entries.size() <= i || 0 == gps_entries[i].info.valid) {
			PACK_BITS(0xFFFFFFFF, packet, base_pos, 21);
			PACK_BITS(0xFFFFFFFF, packet, base_pos, 22);
		} else {
			PACK_BITS(convert_latitude(gps_entries[i].info.lat), packet, base_pos, 21);
			DEBUG_TRACE("ArgosScheduler::build_long_packet: lat[%u]=%u (%lf)", i, convert_latitude(gps_entries[i].info.lat), gps_entries[i].info.lat);
			PACK_BITS(convert_longitude(gps_entries[i].info.lon), packet, base_pos, 22);
			DEBUG_TRACE("ArgosScheduler::build_long_packet: lon[%u]=%u (%lf)", i, convert_longitude(gps_entries[i].info.lon), gps_entries[i].info.lon);
		}
	}

	// Calculate CRC8
#ifdef ARGOS_USE_CRC8
	unsigned char crc8 = CRC8::checksum(packet.substr(LONG_PACKET_HEADER_BYTES+1), LONG_PACKET_PAYLOAD_BITS - 8);
	unsigned int crc_offset = 8*LONG_PACKET_HEADER_BYTES;
	PACK_BITS(crc8, packet, crc_offset, 8);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: crc8=%02x", crc8);
#endif

	// BCH code B255_223_4
	BCHCodeWord code_word = BCHEncoder::encode(
			BCHEncoder::B255_223_4,
			sizeof(BCHEncoder::B255_223_4),
			packet.substr(LONG_PACKET_HEADER_BYTES), LONG_PACKET_PAYLOAD_BITS);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: bch=%08x", code_word);

	// Append BCH code
	PACK_BITS(code_word, packet, base_pos, BCHEncoder::B255_223_4_CODE_LEN);
}

void ArgosScheduler::handle_packet(ArgosPacket const& packet, unsigned int total_bits, const ArgosMode mode) {
	DEBUG_TRACE("ArgosScheduler::handle_packet: bytes=%u total_bits=%u freq=%lf power=%u mode=%u",
			packet.size(), total_bits, m_argos_config.frequency, m_argos_config.power, (unsigned int)mode);
	DEBUG_TRACE("ArgosScheduler::handle_packet: data=");
#if defined(DEBUG_ENABLE) && DEBUG_LEVEL >= 4
	for (unsigned int i = 0; i < packet.size(); i++)
		printf("%02X", (unsigned int)packet[i] & 0xFF);
	printf("\r\n");
#endif
	power_on();
	set_frequency(m_argos_config.frequency);
	set_tx_power(m_argos_config.power);
	if (m_data_notification_callback)
		m_data_notification_callback(ServiceEvent::ARGOS_TX_START);
	send_packet(packet, total_bits, mode);
	if (m_data_notification_callback)
		m_data_notification_callback(ServiceEvent::ARGOS_TX_END);
	power_off();

	// Update the LAST_TX in the configuration store
	std::time_t last_tx = rtc->gettime();
	configuration_store->write_param(ParamID::LAST_TX, last_tx);

	// Increment TX counter
	configuration_store->increment_tx_counter();
}

void ArgosScheduler::periodic_algorithm() {
	unsigned int max_index = (((unsigned int)m_argos_config.depth_pile + MAX_GPS_ENTRIES_IN_PACKET-1) / MAX_GPS_ENTRIES_IN_PACKET);
	unsigned int index = m_msg_index % max_index;
	unsigned int num_entries = sensor_log->num_entries();
	unsigned int span = std::min((unsigned int)MAX_GPS_ENTRIES_IN_PACKET, (unsigned int)m_argos_config.depth_pile);
	ArgosPacket packet;
	GPSLogEntry gps_entry;

	DEBUG_TRACE("ArgosScheduler::periodic_algorithm: msg_index=%u burst_counter=%u span=%u num_log_entries=%u", m_msg_index, m_msg_burst_counter[index], span, num_entries);

	// Mark last schedule attempt
	m_last_transmission_schedule = m_tr_nom_schedule;

	if (m_msg_burst_counter[index] == 0) {
		DEBUG_WARN("ArgosScheduler::periodic_algorithm: burst counter is zero; not transmitting");
		return;
	}

	if (num_entries < (span * (index+1))) {
		DEBUG_WARN("ArgosScheduler::periodic_algorithm: insufficient entries in sensor log; not transmitting");
		return;
	}

	if (span == 1) {
		// Read the GPS entry according to its index
		unsigned int idx = num_entries - index - 1;
		DEBUG_TRACE("read gps_entry=%u", idx);
		sensor_log->read(&gps_entry, idx);
		build_short_packet(gps_entry, packet);
		handle_packet(packet, SHORT_PACKET_BYTES * BITS_PER_BYTE, ArgosMode::ARGOS_2);
	} else {
		std::vector<GPSLogEntry> gps_entries;
		for (unsigned int k = 0; k < span; k++) {
			unsigned int idx = num_entries - (span * (index+1)) + k;
			DEBUG_TRACE("read gps_entry=%u", idx);
			sensor_log->read(&gps_entry, idx);
			gps_entries.push_back(gps_entry);
		}
		build_long_packet(gps_entries, packet);
		handle_packet(packet, LONG_PACKET_BYTES * BITS_PER_BYTE, ArgosMode::ARGOS_2);
	}

	m_msg_burst_counter[index]--;
	m_msg_index++;
}

void ArgosScheduler::start(std::function<void(ServiceEvent)> data_notification_callback) {
	DEBUG_INFO("ArgosScheduler::start");
	m_data_notification_callback = data_notification_callback;
	m_is_running = true;
	m_is_deferred = false;
	m_msg_index = 0;
	m_next_prepass = INVALID_SCHEDULE;
	m_tr_nom_schedule = INVALID_SCHEDULE;
	m_last_transmission_schedule = INVALID_SCHEDULE;
	configuration_store->get_argos_configuration(m_argos_config);
	for (unsigned int i = 0; i < MAX_MSG_INDEX; i++) {
		m_msg_burst_counter[i] = (m_argos_config.ntry_per_message == 0) ? UINT_MAX : m_argos_config.ntry_per_message;
		m_gps_entries[i].clear();
	}
}

void ArgosScheduler::stop() {
	DEBUG_INFO("ArgosScheduler::stop");
	deschedule();
	m_is_running = false;

	power_off();
}

void ArgosScheduler::deschedule() {
	system_scheduler->cancel_task(m_argos_task);
}

void ArgosScheduler::notify_saltwater_switch_state(bool state) {
	DEBUG_TRACE("ArgosScheduler::notify_saltwater_switch_state");
	if (m_is_running && m_argos_config.underwater_en) {
		m_switch_state = state;
		if (!m_switch_state) {
			DEBUG_TRACE("ArgosScheduler::notify_saltwater_switch_state: state=0: rescheduling");
			m_earliest_tx = rtc->gettime() + m_argos_config.dry_time_before_tx;
			reschedule();
		} else {
			DEBUG_TRACE("ArgosScheduler::notify_saltwater_switch_state: state=1: deferring schedule");
			deschedule();
			m_is_deferred = true;
		}
	}
}
