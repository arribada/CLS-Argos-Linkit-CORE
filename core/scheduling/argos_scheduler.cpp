#include <algorithm>
#include <climits>
#include <iomanip>
#include <ctime>
#include <cstdlib>

#include "messages.hpp"
#include "argos_scheduler.hpp"
#include "rtc.hpp"
#include "config_store.hpp"
#include "scheduler.hpp"
#include "bitpack.hpp"
#include "timeutils.hpp"
#include "bch.hpp"
#include "crc8.hpp"
#include "binascii.hpp"
#include "battery.hpp"
#include "dte_protocol.hpp"

extern "C" {
	#include "previpass.h"
}

#define INVALID_SCHEDULE    (uint64_t)-1

#define TX_JITTER_MS		5000

#define FIXTYPE_3D			3

#define HOURS_PER_DAY       24
#define SECONDS_PER_MINUTE	60
#define SECONDS_PER_HOUR    3600
#define SECONDS_PER_DAY     (SECONDS_PER_HOUR * HOURS_PER_DAY)
#define MM_PER_METER		1000
#define MM_PER_KM   		1000000
#define MV_PER_UNIT			20
#define MS_PER_SEC			1000
#define METRES_PER_UNIT     40
#define DEGREES_PER_UNIT	(1.0f/1.42f)
#define BITS_PER_BYTE		8
#define MIN_ALTITUDE		0
#define MAX_ALTITUDE		254
#define INVALID_ALTITUDE	255

#define LON_LAT_RESOLUTION  10000

#define MAX_GPS_ENTRIES_IN_PACKET	4

#define SHORT_PACKET_BITS   		120
#define SHORT_PACKET_PAYLOAD_BITS   99
#define SHORT_PACKET_BYTES			15

#define LONG_PACKET_BITS   			248
#define LONG_PACKET_PAYLOAD_BITS   	216
#define LONG_PACKET_BYTES			31

#define DOPPLER_PACKET_BITS   		24
#define DOPPLER_PACKET_PAYLOAD_BITS 24
#define DOPPLER_PACKET_BYTES		3

// The margin is the time advance supplied when in prepass mode for TX and RX operations
// It represents the time taken for programming tasks, etc.  We advance the RX task by 3
// seconds extra relative to the TX task to avoid race conditions when the two might
// fire at the same time otherwise.
#define ARGOS_TX_MARGIN_SECS        0  // Don't try to compensate TX
#define ARGOS_RX_MARGIN_SECS        0  // RX always follows TX

// TX certification power off threshold (in seconds)
#define CERT_TX_POWER_OFF_REPETITION_THRESHOLD  15

extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;
extern RTC       *rtc;
extern Logger    *sensor_log;
extern BatteryMonitor *battery_monitor;
extern Timer          *system_timer;


ArgosScheduler::ArgosScheduler() {
	m_is_running = false;
	m_switch_state = false;
	m_earliest_tx = INVALID_SCHEDULE;
	m_last_longitude = INVALID_GEODESIC;
	m_last_latitude = INVALID_GEODESIC;
	m_next_schedule_relative = 0;
}

void ArgosScheduler::process_rx() {

	// If we are in TX certification mode, don't power off the device if the
	// TX repetition period is 15 seconds or less to avoid re-loading time
	if (m_argos_config.cert_tx_enable) {
		DEBUG_TRACE("ArgosScheduler::process_rx: TX certification is on");
		if (m_argos_config.cert_tx_repetition > CERT_TX_POWER_OFF_REPETITION_THRESHOLD)
			power_off();
		return;
	}

	if (m_is_tx_pending) {
		DEBUG_TRACE("ArgosScheduler::process_rx: DL RX deferred while TX pending");
		return;
	}

	if (!m_argos_config.argos_rx_en) {
		DEBUG_TRACE("ArgosScheduler::process_rx: ARGOS_RX_EN is off");
		power_off();
		return;
	}

	if (m_downlink_end == INVALID_SCHEDULE) {
		DEBUG_TRACE("ArgosScheduler::process_rx: prepass window DL not configured");
		power_off();
		return;
	}

	time_t now = rtc->gettime();
	if ((uint64_t)now >= m_downlink_end) {
		DEBUG_INFO("ArgosScheduler::process_rx: Setting RX_OFF: window has elapsed");
		m_downlink_end = INVALID_SCHEDULE;
		power_off();
		return;
	}

	if (now < (m_argos_config.last_aop_update + (SECONDS_PER_DAY * m_argos_config.argos_rx_aop_update_period))) {
		DEBUG_TRACE("ArgosScheduler::process_rx: AOP update not needed for another %lu secs",
				(m_argos_config.last_aop_update + (SECONDS_PER_DAY * m_argos_config.argos_rx_aop_update_period)) - now);
		power_off();
		return;
	}

	if ((uint64_t)now >= m_downlink_start) {
		DEBUG_INFO("ArgosScheduler::process_rx: Setting RX_ON for %.3f secs", (double)m_downlink_end - now);
		set_rx_mode(ArgosMode::ARGOS_3, m_downlink_end);
	}
}


void ArgosScheduler::reschedule() {
	uint64_t schedule = INVALID_SCHEDULE;

	// Obtain fresh copy of configuration as it may have changed
	BaseArgosMode last_mode = m_argos_config.mode;
	configuration_store->get_argos_configuration(m_argos_config);

	// Check to see if we are already scheduled and no mode change has arisen
	if (system_scheduler->is_scheduled(m_tx_task) && last_mode == m_argos_config.mode) {
		DEBUG_TRACE("ArgosScheduler::reschedule: already scheduled in %.3f secs",
				((double)m_next_schedule_absolute / MS_PER_SEC) - (double)rtc->gettime());
		return;
	}

	if (m_argos_config.cert_tx_enable) {
		DEBUG_TRACE("ArgosScheduler: certification TX mode");
		schedule = next_certification_tx();
	} else if (m_argos_config.mode == BaseArgosMode::OFF) {
		DEBUG_WARN("ArgosScheduler: mode is OFF -- not scheduling");
		return;
	} else if (m_argos_config.gnss_en && !rtc->is_set()) {
		DEBUG_WARN("ArgosScheduler: RTC is not yet set -- not scheduling");
		return;
	} else if (m_argos_config.time_sync_burst_en && !m_time_sync_burst_sent && m_num_gps_entries) {
		// Schedule an immediate time sync burst
		DEBUG_TRACE("ArgosScheduler: scheduling immediate time sync burst");
		schedule = 0;
	} else if (m_argos_config.mode == BaseArgosMode::LEGACY) {
		// In legacy mode we schedule every hour aligned to UTC
		schedule = next_duty_cycle(0xFFFFFFU);
	} else if (m_argos_config.mode == BaseArgosMode::DUTY_CYCLE) {
		schedule = next_duty_cycle(m_argos_config.duty_cycle);
	} else if (m_argos_config.gnss_en && m_argos_config.mode == BaseArgosMode::PASS_PREDICTION) {
		schedule = next_prepass();
	} else {
		DEBUG_WARN("ArgosScheduler: Invalid argos mode configuration");
	}

	if (INVALID_SCHEDULE != schedule) {
		DEBUG_TRACE("ArgosScheduler: TX schedule in: %.3f secs", (double)schedule / MS_PER_SEC);
		deschedule();
		m_tx_task = system_scheduler->post_task_prio(std::bind(&ArgosScheduler::process_schedule, this),
				"ArgosSchedulerProcessSchedule",
				Scheduler::DEFAULT_PRIORITY, schedule);
		m_next_schedule_relative = schedule;
	} else {
		DEBUG_INFO("ArgosScheduler: not rescheduling");
	}
}

static inline bool is_in_duty_cycle(uint64_t time_ms, unsigned int duty_cycle)
{
	// Note that duty cycle is a bit-field comprising 24 bits as follows:
	// 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00  bit
	// 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 21 21 22 23  hour (UTC)
	uint64_t msec_of_day = (time_ms % (SECONDS_PER_DAY * MS_PER_SEC));
	unsigned int hour_of_day = msec_of_day / (SECONDS_PER_HOUR * MS_PER_SEC);
	return (duty_cycle & (0x800000 >> hour_of_day));
}

uint64_t ArgosScheduler::next_certification_tx()
{
	uint64_t now = system_timer->get_counter();
	if (INVALID_SCHEDULE == m_last_transmission_schedule) {
		m_next_schedule_absolute = now;
	} else {
		m_next_schedule_absolute = m_last_transmission_schedule + (m_argos_config.cert_tx_repetition * MS_PER_SEC);
	}

	DEBUG_TRACE("ArgosScheduler::next_certification_tx: tx_rep=%u now=%llu next=%llu", m_argos_config.cert_tx_repetition, now, m_next_schedule_absolute);

	if (now > m_next_schedule_absolute)
		return 0;

	return m_next_schedule_absolute - now;
}

uint64_t ArgosScheduler::next_duty_cycle(unsigned int duty_cycle)
{
	// Do not proceed unless duty cycle is non-zero to save time
	if (duty_cycle == 0)
	{
		DEBUG_INFO("ArgosScheduler::next_duty_cycle: no schedule found as duty cycle is zero!");
		m_next_schedule_absolute = INVALID_SCHEDULE;
		return INVALID_SCHEDULE;
	}

	// Find epoch time for start of the "current" day
	uint64_t now = rtc->gettime();

	DEBUG_TRACE("ArgosScheduler::next_duty_cycle: now=%llu next=%.3f last=%.3f", now,
			(double)m_next_schedule_absolute / MS_PER_SEC,
			(double)m_last_transmission_schedule / MS_PER_SEC);

	// Handle the case where SWS has set earliest TX time
	if (m_earliest_tx != INVALID_SCHEDULE)
	{
		// If earliest TX is in the future try to schedule it for that point
		if (m_earliest_tx > now) {
			// Make sure start_time is sufficiently advanced from the last TX
			uint64_t start_time = m_earliest_tx * MS_PER_SEC;
			if (m_last_transmission_schedule != INVALID_SCHEDULE &&
				start_time < m_last_transmission_schedule)
				start_time = m_last_transmission_schedule;

			// Check if start_time is in the duty cycle
			DEBUG_TRACE("ArgosScheduler::next_duty_cycle: earliest TX is in %llu secs", m_earliest_tx - now);
			if (is_in_duty_cycle(start_time, duty_cycle)) {
				return start_time - (now * MS_PER_SEC);
			} else {
				// Not in duty cycle, so allow a new schedule to be computed
				m_next_schedule_absolute = start_time;
			}
		}
	}

	// A new TR_NOM schedule is required

	// Set schedule based on last TR_NOM point (if there is one)
	if (m_last_transmission_schedule == INVALID_SCHEDULE) {
		// Use now as the initial TR_NOM -- we don't allow
		// a -ve jitter amount in this case to avoid a potential -ve overflow
		uint64_t start_time = now * MS_PER_SEC;
		update_tx_jitter(0, TX_JITTER_MS);
		m_next_schedule_absolute = start_time + m_tx_jitter;
	}
	else
	{
		// Advance by TR_NOM + TX jitter if we have a previous TR_NOM
		// It should be safe to allow a -ve jitter because TR_NOM is always larger
		// than the jitter amount
		m_next_schedule_absolute = m_last_transmission_schedule;
		update_tx_jitter(-TX_JITTER_MS, TX_JITTER_MS);
		m_next_schedule_absolute += (m_argos_config.tr_nom * MS_PER_SEC) + m_tx_jitter;
	}

	DEBUG_TRACE("ArgosScheduler::next_duty_cycle: starting m_tr_nom_schedule = %.3f", (double)m_next_schedule_absolute / MS_PER_SEC);

	// We iterate forwards from the candidate m_tr_nom_schedule until we find a TR_NOM that
	// falls inside a permitted hour of transmission.  The maximum span we search is 24 hours.
	uint64_t elapsed_time = 0;
	while (elapsed_time <= (MS_PER_SEC * SECONDS_PER_DAY)) {
		//DEBUG_TRACE("ArgosScheduler::next_duty_cycle: now: %lu candidate schedule: %.3f elapsed: %.3f", now, (double)m_tr_nom_schedule / MS_PER_SEC, (double)elapsed_time / MS_PER_SEC);
		if (is_in_duty_cycle(m_next_schedule_absolute, duty_cycle) && m_next_schedule_absolute >= (now * MS_PER_SEC)) {
			DEBUG_TRACE("ArgosScheduler::next_duty_cycle: found schedule in %.3f seconds", (double)(m_next_schedule_absolute - (now * MS_PER_SEC)) / MS_PER_SEC);
			m_next_mode = ArgosMode::ARGOS_2;
			return (m_next_schedule_absolute - (now * MS_PER_SEC));
		} else {
			uint64_t delta;
			update_tx_jitter(-TX_JITTER_MS, TX_JITTER_MS);
			delta = (m_argos_config.tr_nom * MS_PER_SEC);
			delta += m_tx_jitter;
			m_next_schedule_absolute += delta;
			elapsed_time += delta;
		}
	}

	DEBUG_ERROR("ArgosScheduler::next_duty_cycle: no schedule found!");

	m_next_schedule_absolute = INVALID_SCHEDULE;

	return INVALID_SCHEDULE;
}

uint64_t ArgosScheduler::next_prepass() {

	DEBUG_TRACE("ArgosScheduler::next_prepass");

	// We must have a previous GPS location to proceed
	if (m_last_latitude == INVALID_GEODESIC) {
		DEBUG_WARN("ArgosScheduler::next_prepass: current GPS location is not presently known - aborting");
		return INVALID_SCHEDULE;
	}

	uint64_t curr_time = rtc->gettime();

	// Assume start window position is current time
	std::time_t start_time = curr_time;

	// If we were deferred by SWS then recompute using a start window that reflects earliest TX
	if (m_earliest_tx != INVALID_SCHEDULE &&
		m_earliest_tx >= curr_time) {
		DEBUG_TRACE("ArgosScheduler::next_prepass: rescheduling after SWS interruption earliest TX in %llu secs", m_earliest_tx - curr_time);
		start_time = m_earliest_tx;
	}

	// Set start and end time of the prepass search - we use a 24 hour window
	std::time_t stop_time = start_time + (std::time_t)(24 * SECONDS_PER_HOUR);
	struct tm *p_tm = std::gmtime(&start_time);
	struct tm tm_start = *p_tm;
	p_tm = std::gmtime(&stop_time);
	struct tm tm_stop = *p_tm;

	DEBUG_INFO("ArgosScheduler::next_prepass: searching window start=%llu now=%llu stop=%llu", start_time, curr_time, stop_time);

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

	// Check for next RX window if we don't have one yet
	if (m_downlink_end == INVALID_SCHEDULE && m_argos_config.argos_rx_en &&
        curr_time >= (uint64_t)(m_argos_config.last_aop_update + (SECONDS_PER_DAY * m_argos_config.argos_rx_aop_update_period)))
	{
		if (PREVIPASS_compute_next_pass_with_status(
	    	&config,
			pass_predict.records,
			pass_predict.num_records,
			SAT_DNLK_ON_WITH_A3,
			SAT_UPLK_OFF,
			&next_pass)) {
			// Mark a new downlink RX window
			m_downlink_end = (uint64_t)next_pass.epoch + next_pass.duration;
			m_downlink_start = (uint64_t)next_pass.epoch;
			DEBUG_INFO("next_prepass: new DL RX window = [%llu, %llu]", m_downlink_start, m_downlink_end);
		}
	}

	while (PREVIPASS_compute_next_pass(
    		&config,
			pass_predict.records,
			pass_predict.num_records,
			&next_pass)) {

		// No schedule
		uint64_t schedule = 0;

		// If there is a previous transmission then make sure schedule is at least advance TR_NOM
		if (m_last_transmission_schedule != INVALID_SCHEDULE)
			schedule = std::max((uint64_t)schedule, m_last_transmission_schedule + (m_argos_config.tr_nom * MS_PER_SEC));

		// Advance to at least the prepass epoch position
		schedule = std::max(((uint64_t)next_pass.epoch * MS_PER_SEC), schedule);

		// Apply nominal jitter to schedule
		// NOTE: Because of the possibility of -ve jitter resulting in an edge case we have to make
		// sure the schedule is advanced passed at least start_time and curr_time
		update_tx_jitter(-TX_JITTER_MS, TX_JITTER_MS);
		schedule += m_tx_jitter;

		// Make sure computed schedule is at least start_time (which could have been set by SWS)
		schedule = std::max(((uint64_t)start_time * MS_PER_SEC), schedule);

		// Make sure computed schedule is at least current RTC time to avoid a -ve schedule
		schedule = std::max(((uint64_t)curr_time * MS_PER_SEC), schedule);

		DEBUG_INFO("ArgosScheduler::next_prepass: hex_id=%01x dl=%u ul=%u last=%.3f s=%.3f c=%.3f e=%.3f",
					(unsigned int)next_pass.satHexId,
					(unsigned int)next_pass.downlinkStatus,
					(unsigned int)next_pass.uplinkStatus,
					(m_last_transmission_schedule == INVALID_SCHEDULE) ? 0 :
							((double)m_last_transmission_schedule/MS_PER_SEC - curr_time), (double)next_pass.epoch - (double)curr_time,
							((double)schedule / MS_PER_SEC) - curr_time, ((double)next_pass.epoch + (double)next_pass.duration) - curr_time);

		// Check we don't transmit off the end of the prepass window
		if ((schedule + (ARGOS_TX_MARGIN_SECS * MS_PER_SEC)) < ((uint64_t)next_pass.epoch + next_pass.duration) * MS_PER_SEC) {
			// We're good to go for this schedule, compute relative delay until the epoch arrives
			// and set the required Argos transmission mode
			DEBUG_INFO("ArgosScheduler::next_prepass: scheduled for %.3f seconds from now", (double)(schedule - (curr_time * MS_PER_SEC)) / MS_PER_SEC);
			m_next_schedule_absolute = schedule;
			m_next_mode = next_pass.uplinkStatus >= SAT_UPLK_ON_WITH_A3 ? ArgosMode::ARGOS_3 : ArgosMode::ARGOS_2;
			return schedule - (curr_time * MS_PER_SEC);
		} else {
			DEBUG_TRACE("ArgosScheduler::next_prepass: computed schedule is too late for this window", next_pass.epoch, next_pass.duration);
			curr_time = rtc->gettime();
			start_time = (std::time_t)next_pass.epoch + next_pass.duration;
			p_tm = std::gmtime(&start_time);
			tm_start = *p_tm;
			config.start = { (uint16_t)(1900 + tm_start.tm_year), (uint8_t)(tm_start.tm_mon + 1), (uint8_t)tm_start.tm_mday, (uint8_t)tm_start.tm_hour, (uint8_t)tm_start.tm_min, (uint8_t)tm_start.tm_sec };
			DEBUG_INFO("ArgosScheduler::next_prepass: searching window start=%llu now=%llu stop=%llu", start_time, curr_time, stop_time);
		}
	}

	// No passes reported
	DEBUG_ERROR("ArgosScheduler::next_prepass: PREVIPASS_compute_next_pass returned no passes");
	return INVALID_SCHEDULE;
}

void ArgosScheduler::process_schedule() {

	uint64_t now = rtc->gettime();
	if (!m_switch_state && (m_earliest_tx == INVALID_SCHEDULE || m_earliest_tx <= now)) {
		if (m_argos_config.cert_tx_enable) {
			prepare_certification_burst();
		} else if (m_argos_config.time_sync_burst_en && !m_time_sync_burst_sent && m_num_gps_entries && rtc->is_set()) {
			prepare_time_sync_burst();
		} else if (!m_argos_config.gnss_en) {
			prepare_doppler_burst();
		} else {
			prepare_normal_burst();
		}
	} else {
		DEBUG_TRACE("ArgosScheduler::process_schedule: sws=%u t=%llu earliest_tx=%llu deferring transmission", m_switch_state, now, m_earliest_tx);
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
			DEBUG_TRACE("ArgosScheduler::notify_sensor_log_update: updated last known GPS position; is_rtc_set=%u @ %u/%u/%u %02u:%02u:%02u",
					rtc->is_set(),
					(unsigned int)gps_entry.info.year, (unsigned int)gps_entry.info.month, (unsigned int)gps_entry.info.day,
					(unsigned int)gps_entry.info.hour, (unsigned int)gps_entry.info.min, (unsigned int)gps_entry.info.sec);
			m_last_longitude = gps_entry.info.lon;
			m_last_latitude = gps_entry.info.lat;
		}

		// Keep a running track of number of GPS entries in local cache
		// Note that the cache is dimensioned to the maximum depth pile size of 24
		// but only the "last N" are used when preparing a burst based on the argos config
		{
			// Update the GPS map based on the most recent entry

			// If mode is duty cycle or legacy then also force ntry per message to infinite
			m_gps_entry_burst_counter[m_num_gps_entries] = (m_argos_config.ntry_per_message == 0 || m_argos_config.mode == BaseArgosMode::DUTY_CYCLE || m_argos_config.mode == BaseArgosMode::LEGACY) ? UINT_MAX : m_argos_config.ntry_per_message;

			// Copy the entry into local map
			m_gps_log_entry[m_num_gps_entries] = gps_entry;

			// Update our local count of available GPS entries (also acts as a map key and guaranteed to be unique)
			m_num_gps_entries++;

			// If the number of GPS entries exceeds the max depth pile then delete the oldest entry from the GPS map so it
			// doesn't keep growing in size.
			if (m_num_gps_entries > (unsigned int)BaseArgosDepthPile::DEPTH_PILE_24) {
				unsigned int index = m_num_gps_entries - (unsigned int)BaseArgosDepthPile::DEPTH_PILE_24 - 1;
				DEBUG_TRACE("ArgosScheduler::notify_sensor_log_update: erasing entry %u from depth pile", index);
				m_gps_entry_burst_counter.erase(index);
				m_gps_log_entry.erase(index);
			}

			DEBUG_TRACE("ArgosScheduler::notify_sensor_log_update: depth pile has %u/24 entries with total %u seen", m_gps_entry_burst_counter.size(), m_num_gps_entries);
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

unsigned int ArgosScheduler::build_certification_packet(ArgosPacket& packet) {

	unsigned int sz;

	// Convert from ASCII hex to a real binary buffer
	packet = Binascii::unhexlify(m_argos_config.cert_tx_payload);

	DEBUG_TRACE("ArgosScheduler::build_certification_packet: TX payload size %u bytes", packet.size());

	// Check the size to determine the packet #bits to send in payload
	if (packet.size() > SHORT_PACKET_BYTES) {
		DEBUG_TRACE("ArgosScheduler::build_certification_packet: Using long packet");
		sz = LONG_PACKET_BITS;
		packet.resize(LONG_PACKET_BYTES);
	} else {
		DEBUG_TRACE("ArgosScheduler::build_certification_packet: Using short packet");
		sz = SHORT_PACKET_BITS;
		packet.resize(SHORT_PACKET_BYTES);
	}

	switch (m_argos_config.cert_tx_modulation) {
	case BaseArgosModulation::A2:
		m_next_mode = ArgosMode::ARGOS_2;
		break;
	case BaseArgosModulation::A3:
		m_next_mode = ArgosMode::ARGOS_3;
		break;
	case BaseArgosModulation::A4:
	default:
		DEBUG_WARN("ArgosScheduler::build_certification_packet: modulation mode %u not supported, using A2 instead", m_argos_config.cert_tx_modulation);
		m_next_mode = ArgosMode::ARGOS_2;
		break;
	}

	return sz;
}

void ArgosScheduler::build_doppler_packet(ArgosPacket& packet) {

	DEBUG_TRACE("ArgosScheduler::build_doppler_packet");
	unsigned int base_pos = 0;

	// Reserve required number of bytes
	packet.assign(DOPPLER_PACKET_BYTES, 0);

	// Payload bytes
	PACK_BITS(0, packet, base_pos, 8);  // Zero CRC field (computed later)

	unsigned int last_known_pos = 0;
	PACK_BITS(last_known_pos, packet, base_pos, 8);
	DEBUG_TRACE("ArgosScheduler::build_doppler_packet: last_known_pos=%u", (unsigned int)last_known_pos);

	unsigned int batt_voltage = battery_monitor->get_voltage();
	unsigned int batt = std::min(127, std::max((int)batt_voltage - 2700, (int)0) / MV_PER_UNIT);
	PACK_BITS(batt, packet, base_pos, 7);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: voltage=%u (%u)", (unsigned int)batt, (unsigned int)batt_voltage);

	// LOWBATERY_FLAG
	PACK_BITS(m_argos_config.is_lb, packet, base_pos, 1);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: is_lb=%u", (unsigned int)m_argos_config.is_lb);

	// Calculate CRC8
	unsigned char crc8 = CRC8::checksum(packet.substr(1), DOPPLER_PACKET_PAYLOAD_BITS - 8);
	unsigned int crc_offset = 0;
	PACK_BITS(crc8, packet, crc_offset, 8);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: crc8=%02x", crc8);
}

void ArgosScheduler::build_short_packet(GPSLogEntry const& gps_entry, ArgosPacket& packet) {

	DEBUG_TRACE("ArgosScheduler::build_short_packet");
	unsigned int base_pos = 0;

	// Reserve required number of bytes
	packet.assign(SHORT_PACKET_BYTES, 0);

	// Payload bytes
	PACK_BITS(0, packet, base_pos, 8);  // Zero CRC field (computed later)

	// Use scheduled GPS time as day/hour/min
	uint16_t year;
	uint8_t month, day, hour, min, sec;
	convert_datetime_to_epoch(gps_entry.info.schedTime, year, month, day, hour, min, sec);
	PACK_BITS(gps_entry.info.day, packet, base_pos, 5);

	DEBUG_TRACE("ArgosScheduler::build_short_packet: day=%u", (unsigned int)day);
	PACK_BITS(gps_entry.info.hour, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: hour=%u", (unsigned int)hour);
	PACK_BITS(gps_entry.info.min, packet, base_pos, 6);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: min=%u", (unsigned int)min);

	if (gps_entry.info.valid) {
		PACK_BITS(convert_latitude(gps_entry.info.lat), packet, base_pos, 21);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: lat=%u (%lf)", convert_latitude(gps_entry.info.lat), gps_entry.info.lat);
		PACK_BITS(convert_longitude(gps_entry.info.lon), packet, base_pos, 22);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: lon=%u (%lf)", convert_longitude(gps_entry.info.lon), gps_entry.info.lon);
		double gspeed = (SECONDS_PER_HOUR * gps_entry.info.gSpeed) / (2*MM_PER_KM);
		PACK_BITS((unsigned int)gspeed, packet, base_pos, 7);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: speed=%u", (unsigned int)gspeed);

		// OUTOFZONE_FLAG
		PACK_BITS(m_argos_config.is_out_of_zone, packet, base_pos, 1);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: is_out_of_zone=%u", m_argos_config.is_out_of_zone);

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
		PACK_BITS(0xFF, packet, base_pos, 7);
		PACK_BITS(m_argos_config.is_out_of_zone, packet, base_pos, 1);
		DEBUG_TRACE("ArgosScheduler::build_short_packet: is_out_of_zone=%u", (unsigned int)m_argos_config.is_out_of_zone);
		PACK_BITS(0xFF, packet, base_pos, 8);
		PACK_BITS(0xFF, packet, base_pos, 8);
	}

	unsigned int batt = std::min(127, std::max((int)gps_entry.info.batt_voltage - 2700, (int)0) / MV_PER_UNIT);
	PACK_BITS(batt, packet, base_pos, 7);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: voltage=%u (%u)", (unsigned int)batt, (unsigned int)gps_entry.info.batt_voltage);

	// LOWBATERY_FLAG
	PACK_BITS(m_argos_config.is_lb, packet, base_pos, 1);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: is_lb=%u", (unsigned int)m_argos_config.is_lb);

	// Calculate CRC8
	unsigned char crc8 = CRC8::checksum(packet.substr(1), SHORT_PACKET_PAYLOAD_BITS - 8);
	unsigned int crc_offset = 0;
	PACK_BITS(crc8, packet, crc_offset, 8);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: crc8=%02x", crc8);

	// BCH code B127_106_3
	BCHCodeWord code_word = BCHEncoder::encode(
			BCHEncoder::B127_106_3,
			sizeof(BCHEncoder::B127_106_3),
			packet, SHORT_PACKET_PAYLOAD_BITS);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: bch=%06x", code_word);

	// Append BCH code
	PACK_BITS(code_word, packet, base_pos, BCHEncoder::B127_106_3_CODE_LEN);
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

	// Payload bytes
	PACK_BITS(0, packet, base_pos, 8);  // Zero CRC field (computed later)

	// This will set the log time for the GPS entry based on when it was scheduled
	uint16_t year;
	uint8_t month, day, hour, min, sec;
	convert_datetime_to_epoch(gps_entries[0].info.schedTime, year, month, day, hour, min, sec);

	PACK_BITS(day, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: day=%u", (unsigned int)day);
	PACK_BITS(hour, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: hour=%u", (unsigned int)hour);
	PACK_BITS(min, packet, base_pos, 6);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: min=%u", (unsigned int)min);

	// First GPS entry
	if (gps_entries[0].info.valid) {
		PACK_BITS(convert_latitude(gps_entries[0].info.lat), packet, base_pos, 21);
		DEBUG_TRACE("ArgosScheduler::build_long_packet: lat=%u (%lf)", convert_latitude(gps_entries[0].info.lat), gps_entries[0].info.lat);
		PACK_BITS(convert_longitude(gps_entries[0].info.lon), packet, base_pos, 22);
		DEBUG_TRACE("ArgosScheduler::build_long_packet: lon=%u (%lf)", convert_longitude(gps_entries[0].info.lon), gps_entries[0].info.lon);
		double gspeed = (SECONDS_PER_HOUR * gps_entries[0].info.gSpeed) / (2*MM_PER_KM);
		PACK_BITS((unsigned int)gspeed, packet, base_pos, 7);
		DEBUG_TRACE("ArgosScheduler::build_long_packet: speed=%u", (unsigned int)gspeed);
	} else {
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 21);
		PACK_BITS(0xFFFFFFFF, packet, base_pos, 22);
		PACK_BITS(0xFF, packet, base_pos, 7);
	}

	// OUTOFZONE_FLAG
	PACK_BITS(m_argos_config.is_out_of_zone, packet, base_pos, 1);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: is_out_of_zone=%u", m_argos_config.is_out_of_zone);

	unsigned int batt = std::min(127, std::max((int)gps_entries[0].info.batt_voltage - 2700, (int)0) / MV_PER_UNIT);
	PACK_BITS(batt, packet, base_pos, 7);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: voltage=%u (%u)", (unsigned int)batt, (unsigned int)gps_entries[0].info.batt_voltage);

	// LOWBATERY_FLAG
	PACK_BITS(m_argos_config.is_lb, packet, base_pos, 1);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: is_lb=%u", (unsigned int)m_argos_config.is_lb);

	// Delta time loc
	PACK_BITS((unsigned int)m_argos_config.delta_time_loc, packet, base_pos, 4);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: delta_time_loc=%u", (unsigned int)m_argos_config.delta_time_loc);

	// Subsequent GPS entries
	for (unsigned int i = 1; i < MAX_GPS_ENTRIES_IN_PACKET; i++) {
		DEBUG_TRACE("ArgosScheduler::build_long_packet: gps_valid[%u]=%u", i, (unsigned int)gps_entries[i].info.valid);
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
	unsigned char crc8 = CRC8::checksum(packet.substr(1), LONG_PACKET_PAYLOAD_BITS - 8);
	unsigned int crc_offset = 0;
	PACK_BITS(crc8, packet, crc_offset, 8);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: crc8=%02x", crc8);

	// BCH code B255_223_4
	BCHCodeWord code_word = BCHEncoder::encode(
			BCHEncoder::B255_223_4,
			sizeof(BCHEncoder::B255_223_4),
			packet, LONG_PACKET_PAYLOAD_BITS);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: bch=%08x", code_word);

	// Append BCH code
	PACK_BITS(code_word, packet, base_pos, BCHEncoder::B255_223_4_CODE_LEN);
}

void ArgosScheduler::handle_packet(ArgosPacket const& packet, unsigned int total_bits, const ArgosMode mode) {
	DEBUG_INFO("ArgosScheduler::handle_packet: battery=%.2lfV freq=%lf power=%s mode=%s",
			   (double)battery_monitor->get_voltage() / 1000, m_argos_config.frequency, argos_power_to_string(m_argos_config.power), argos_mode_to_string(mode));
	DEBUG_INFO("ArgosScheduler::handle_packet: data=%s", Binascii::hexlify(packet).c_str());

	// Store parameters for deferred processing
	m_packet = packet;
	m_total_bits = total_bits;
	m_mode = mode;

	// Ensure the device is powered on and request transmission
	m_is_tx_pending = true;
	power_on(m_argos_config.argos_id, [this](ArgosAsyncEvent e) { handle_event(e); });
	set_frequency(m_argos_config.frequency);
	set_tx_power(m_argos_config.power);
	set_tcxo_warmup_time(m_argos_config.argos_tcxo_warmup_time);
	send_packet(packet, total_bits, mode);
}

void ArgosScheduler::prepare_certification_burst() {
	// Mark last schedule attempt
	m_last_transmission_schedule = m_next_schedule_absolute;

	ArgosPacket packet;
	unsigned int sz = build_certification_packet(packet);
	handle_packet(packet, sz, m_next_mode);
}

void ArgosScheduler::prepare_time_sync_burst() {

	DEBUG_TRACE("ArgosScheduler::prepare_time_sync_burst: num_gps_entries=%u", m_num_gps_entries);

	// Mark the time sync burst as sent because otherwise we would enter an infinite rescheduling scenario
	m_time_sync_burst_sent = true;
	m_last_transmission_schedule = rtc->gettime() * MS_PER_SEC;

	if (m_num_gps_entries) {
		ArgosPacket packet;
		unsigned int index = m_num_gps_entries - 1;

		build_short_packet(m_gps_log_entry.at(index), packet);
		handle_packet(packet, SHORT_PACKET_BITS, ArgosMode::ARGOS_2);

	} else {
		DEBUG_ERROR("ArgosScheduler::prepare_time_sync_burst: sensor log state is invalid, can't transmit");
	}
}

void ArgosScheduler::prepare_doppler_burst() {
	DEBUG_TRACE("ArgosScheduler::prepare_doppler_burst");

	// Mark last schedule attempt
	m_last_transmission_schedule = m_next_schedule_absolute;

	ArgosPacket packet;
	build_doppler_packet(packet);
	handle_packet(packet, DOPPLER_PACKET_BITS, m_next_mode);
}

void ArgosScheduler::prepare_normal_burst() {
	unsigned int max_index = (((unsigned int)m_argos_config.depth_pile + MAX_GPS_ENTRIES_IN_PACKET-1) / MAX_GPS_ENTRIES_IN_PACKET);
	unsigned int index = m_msg_index % max_index;
	unsigned int span = std::min((unsigned int)MAX_GPS_ENTRIES_IN_PACKET, (unsigned int)m_argos_config.depth_pile);
	unsigned int eligible_gps_count = 0;
	unsigned int first_eligible_gps_index = -1;
	unsigned int max_msg_index;
	ArgosPacket packet;

	// Obtain fresh copy of configuration as it may have changed
	configuration_store->get_argos_configuration(m_argos_config);

	DEBUG_TRACE("ArgosScheduler::prepare_normal_burst: msg_index=%u/%u span=%u num_log_entries=%u", m_msg_index % max_index, max_index, span, m_num_gps_entries);

	// Mark last schedule attempt
	if (m_argos_config.mode == BaseArgosMode::PASS_PREDICTION) {
		m_last_transmission_schedule = rtc->gettime() * MS_PER_SEC;
	} else
		m_last_transmission_schedule = m_next_schedule_absolute;

	// If the number of GPS entries received is less than the span then modify span
	// to reflect number of entries available
	if (m_num_gps_entries < span)
		span = m_num_gps_entries;

	// Find first eligible slot for transmission
	max_msg_index = m_msg_index + max_index;
	while (m_msg_index < max_msg_index) {
		index = m_msg_index % max_index;
		// Check to see if any GPS entry has a non-zero burst counter
		for (unsigned int k = 0; k < span; k++) {
			unsigned int idx = m_num_gps_entries - (span * (index+1)) + k;
			if (m_gps_entry_burst_counter.count(idx)) {
				if (m_gps_entry_burst_counter.at(idx)) {
					eligible_gps_count++;
					if (first_eligible_gps_index == (unsigned int)-1)
						first_eligible_gps_index = idx;
				}
			} else
				break;
		}

		// Ensure the entire slot has at least one eligible entry
		if (eligible_gps_count) {
			DEBUG_TRACE("ArgosScheduler::prepare_normal_burst: found %u eligible GPS entries in slot %u", eligible_gps_count, index);
			break;
		}

		m_msg_index++;
	}

	// No further action if no slot is eligible for transmission
	if (m_msg_index == max_msg_index) {
		DEBUG_WARN("ArgosScheduler::prepare_normal_burst: no eligible slot found");
		return;
	}

	// Handle short packet case
	if (eligible_gps_count == 1) {
		// If only a single eligible GPS, then use first_eligible_gps_index
		DEBUG_TRACE("ArgosScheduler::prepare_normal_burst: using short packet for log entry %u bursts remaining %u",
				first_eligible_gps_index, m_gps_entry_burst_counter.at(first_eligible_gps_index));

		// Decrement GPS entry burst counter
		if (m_gps_entry_burst_counter.at(first_eligible_gps_index) > 0)
			m_gps_entry_burst_counter.at(first_eligible_gps_index)--;

		build_short_packet(m_gps_log_entry.at(first_eligible_gps_index), packet);
		handle_packet(packet, SHORT_PACKET_BITS, m_next_mode);
	} else {

		DEBUG_TRACE("ArgosScheduler::prepare_normal_burst: using long packet");

		std::vector<GPSLogEntry> gps_entries;
		for (unsigned int k = 0; k < span; k++) {
			unsigned int idx = m_num_gps_entries - (span * (index+1)) + k;
			DEBUG_TRACE("read gps_entry=%u bursts remaining %u", idx, m_gps_entry_burst_counter.at(idx));
			gps_entries.push_back(m_gps_log_entry.at(idx));

			// Decrement GPS entry burst counter
			if (m_gps_entry_burst_counter.at(idx) > 0)
				m_gps_entry_burst_counter.at(idx)--;
		}

		// GPS entries must be constructed in reverse order (latest -> earliest)
		std::reverse(gps_entries.begin(), gps_entries.end());

		build_long_packet(gps_entries, packet);
		handle_packet(packet, LONG_PACKET_BITS, m_next_mode);
	}

	// Increment for next message slot index
	m_msg_index++;
}

void ArgosScheduler::start(std::function<void(ServiceEvent&)> data_notification_callback) {

	DEBUG_INFO("ArgosScheduler::start");

	m_data_notification_callback = data_notification_callback;
	m_is_running = true;
	m_is_tx_pending = false;
	m_is_deferred = false;
	m_time_sync_burst_sent = false;
	m_msg_index = 0;
	m_num_gps_entries = 0;
	m_downlink_start = INVALID_SCHEDULE;
	m_downlink_end = INVALID_SCHEDULE;
	m_next_schedule_absolute = INVALID_SCHEDULE;
	m_last_transmission_schedule = INVALID_SCHEDULE;
	m_gps_entry_burst_counter.clear();
	m_gps_log_entry.clear();
	configuration_store->get_argos_configuration(m_argos_config);

	// Generate TX jitter value
	m_rng = new std::mt19937(m_argos_config.argos_id);
	m_tx_jitter = 0;

	// If GNSS_EN is off then we should schedule now as we won't receive
	// any sensor log updates
	if (!m_argos_config.gnss_en)
		reschedule();
}

void ArgosScheduler::stop() {
	DEBUG_INFO("ArgosScheduler::stop");
	m_is_running = false;
	power_off_immediate();
	deschedule();
	delete m_rng;
}

void ArgosScheduler::deschedule() {
	system_scheduler->cancel_task(m_tx_task);
}

void ArgosScheduler::update_tx_jitter(int min, int max) {
	if (m_argos_config.argos_tx_jitter_en) {
		std::uniform_int_distribution<int> dist(min, max);
		m_tx_jitter = dist(*m_rng);
		DEBUG_TRACE("ArgosScheduler::update_tx_jitter: m_tx_jitter=%d", m_tx_jitter);
	}
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
		}
	}
}

void ArgosScheduler::handle_event(ArgosAsyncEvent event) {
	switch (event) {
	case ArgosAsyncEvent::ON:
		DEBUG_TRACE("ArgosScheduler::handle_event: DEVICE_ON");
		break;
	case ArgosAsyncEvent::OFF:
		DEBUG_TRACE("ArgosScheduler::handle_event: DEVICE_OFF");
		update_rx_time();
		break;
	case ArgosAsyncEvent::TX_STARTED:
		DEBUG_TRACE("ArgosScheduler::handle_event: TX_STARTED");
		if (m_data_notification_callback) {
	    	ServiceEvent e;
	    	e.event_type = ServiceEventType::ARGOS_TX_START;
	    	e.event_data = false;
	        m_data_notification_callback(e);
		}
		break;

	case ArgosAsyncEvent::ACK_DONE:
		break;

	case ArgosAsyncEvent::TX_DONE:
	{
		DEBUG_TRACE("ArgosScheduler::handle_event: TX_DONE");
		m_is_tx_pending = false;

		if (m_data_notification_callback) {
	    	ServiceEvent e;
	    	e.event_type = ServiceEventType::ARGOS_TX_END;
	    	e.event_data = false;
	        m_data_notification_callback(e);
		}

		// Update the LAST_TX in the configuration store
		std::time_t last_tx = rtc->gettime();

		configuration_store->write_param(ParamID::LAST_TX, last_tx);

		// Increment TX counter
		configuration_store->increment_tx_counter();

		// Save configuration params
		configuration_store->save_params();

		// Update the schedule for the next transmission and also any pending RX
		process_rx();
		reschedule();
		break;
	}

	case ArgosAsyncEvent::RX_PACKET:
		DEBUG_TRACE("ArgosScheduler::handle_event: RX_PACKET");
		handle_rx_packet();
		break;

	case ArgosAsyncEvent::RX_TIMEOUT:
		DEBUG_INFO("ArgosScheduler::handle_event: Setting RX_OFF: timeout");
		m_downlink_end = INVALID_SCHEDULE;
		process_rx();
		break;

	case ArgosAsyncEvent::ERROR:
	{
		DEBUG_TRACE("ArgosScheduler::handle_event: ERROR");
		// If an ERROR occurred during TX then just cleanup
		if (m_is_tx_pending) {

			// Mark any TX as disabled
			m_is_tx_pending = false;

			if (m_data_notification_callback) {
				ServiceEvent e;
				e.event_type = ServiceEventType::ARGOS_TX_END;
				e.event_data = false;
				m_data_notification_callback(e);
			}
		}

		// Invalidate any RX window
		m_downlink_end = INVALID_SCHEDULE;

		// Update the schedule for the next transmission and also any pending RX
		process_rx();
		reschedule();
		break;
	}

	default:
		break;
	}
}

void ArgosScheduler::handle_rx_packet() {

	BasePassPredict pass_predict;
	ArgosPacket packet;
	unsigned int length;

	read_packet(packet, length);

	DEBUG_INFO("ArgosScheduler::handle_rx_packet: packet=%s length=%u", Binascii::hexlify(packet).c_str(), length);

	// Increment RX counter
	configuration_store->increment_rx_counter();

	// Save configuration params
	configuration_store->save_params();

	// Attempt to decode the queue of packets
	PassPredictCodec::decode(m_orbit_params_map, m_constellation_status_map, packet, pass_predict);

	// Check to see if any new AOP records were found
	if (pass_predict.num_records)
		update_pass_predict(pass_predict);
}

void ArgosScheduler::update_rx_time(void) {
	uint64_t t = get_rx_time_on() / MS_PER_SEC;
	if (t) {
		DEBUG_TRACE("ArgosScheduler::update_rx_time: RX ran for %llu secs", t);
		configuration_store->increment_rx_time(t);
		configuration_store->save_params();
	}
}

void ArgosScheduler::update_pass_predict(BasePassPredict& new_pass_predict) {

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
				DEBUG_TRACE("ArgosScheduler::update_pass_predict: hexid=%01x dl=%u ul=%u aop=%u",
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

	DEBUG_TRACE("ArgosScheduler::update_pass_predict: received=%u required=%u", num_updated_records, existing_pass_predict.num_records);

	// Check if we received a sufficient number of records
	if (num_updated_records == new_pass_predict.num_records && num_updated_records >= existing_pass_predict.num_records) {
		DEBUG_INFO("ArgosScheduler::update_pass_predict: RX_OFF: committing %u AOP records", num_updated_records);
		configuration_store->write_pass_predict(existing_pass_predict);
		std::time_t new_aop_time = rtc->gettime();
		configuration_store->write_param(ParamID::ARGOS_AOP_DATE, new_aop_time);
		configuration_store->save_params();
		m_orbit_params_map.clear();
		m_constellation_status_map.clear();

		// Clear down the current RX session to stop new packets arriving
		m_downlink_end = INVALID_SCHEDULE;
		process_rx();
	}
}

uint64_t ArgosScheduler::get_next_schedule() {
	return m_next_schedule_relative;
}
