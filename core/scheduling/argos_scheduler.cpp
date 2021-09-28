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

// The margin is the time advance supplied when in prepass mode for TX and RX operations
// It represents the time taken for programming tasks, etc.  We advance the RX task by 3
// seconds extra relative to the TX task to avoid race conditions when the two might
// fire at the same time otherwise.
#define ARGOS_TX_MARGIN_SECS        7
#define ARGOS_RX_MARGIN_SECS        (ARGOS_TX_MARGIN_SECS + 3)

extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;
extern RTC       *rtc;
extern Logger    *sensor_log;
extern BatteryMonitor *battery_monitor;

ArgosScheduler::ArgosScheduler() {
	m_is_running = false;
	m_switch_state = false;
	m_earliest_tx = INVALID_SCHEDULE;
	m_last_longitude = INVALID_GEODESIC;
	m_last_latitude = INVALID_GEODESIC;
	m_next_schedule = 0;
}

void ArgosScheduler::rx_reschedule() {

	if (system_scheduler->is_scheduled(m_rx_task)) {
		DEBUG_TRACE("ArgosScheduler::rx_reschedule: DL RX already scheduled");
		if (is_powered_on())
			power_off();
		return;
	}

	if (!m_argos_config.argos_rx_en) {
		DEBUG_TRACE("ArgosScheduler::rx_reschedule: ARGOS_RX_EN is off");
		if (is_powered_on())
			power_off();
		return;
	}

	if (m_downlink_end == INVALID_SCHEDULE) {
		DEBUG_TRACE("ArgosScheduler::rx_reschedule: prepass window has no DL");
		if (is_powered_on())
			power_off();
		return;
	}

	time_t now = rtc->gettime();
	if ((uint64_t)now >= m_downlink_end) {
		DEBUG_TRACE("ArgosScheduler::rx_reschedule: DL RX window has elapsed");
		m_downlink_end = INVALID_SCHEDULE;
		if (is_powered_on())
			power_off();
		return;
	}

	uint64_t start_delay_sec = ((uint64_t)now >= (m_downlink_start - ARGOS_RX_MARGIN_SECS)) ? 0 : m_downlink_start - ARGOS_RX_MARGIN_SECS - (uint64_t)now;

	DEBUG_TRACE("ArgosScheduler::rx_reschedule: DL RX scheduled in %llu secs", start_delay_sec);
	m_rx_task = system_scheduler->post_task_prio([this, now]() {
		// Check to see if we need to power on
		if (!is_powered_on()) {
			power_on([this](ArgosAsyncEvent event) {
				handle_rx_event(event);
			});
		} else {
			// Already powered on
			unsigned int duration_sec = std::min((unsigned int)(m_downlink_end - now), m_argos_config.argos_rx_max_window);
			set_rx_mode(ArgosMode::ARGOS_3, MS_PER_SEC * duration_sec);
		}
	}, "ScheduleDownlinkReceive", Scheduler::DEFAULT_PRIORITY, start_delay_sec * MS_PER_SEC);

	// Power off if the scheduled RX is in the future
	if (start_delay_sec > 0) {
		if (is_powered_on())
			power_off();
	}
}

void ArgosScheduler::rx_deschedule() {
	system_scheduler->cancel_task(m_rx_task);
}

void ArgosScheduler::reschedule() {
	uint64_t schedule = INVALID_SCHEDULE;

	// Obtain fresh copy of configuration as it may have changed
	configuration_store->get_argos_configuration(m_argos_config);

	if (m_argos_config.mode == BaseArgosMode::OFF) {
		DEBUG_WARN("ArgosScheduler: mode is OFF -- not scheduling");
		return;
	} else if (!rtc->is_set()) {
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
	} else if (m_argos_config.mode == BaseArgosMode::PASS_PREDICTION) {
		schedule = next_prepass();
	}

	if (INVALID_SCHEDULE != schedule) {
		DEBUG_TRACE("ArgosScheduler: TX schedule in: %.3f secs", (double)schedule / MS_PER_SEC);
		deschedule();
		m_tx_task = system_scheduler->post_task_prio(std::bind(&ArgosScheduler::process_schedule, this),
				"ArgosSchedulerProcessSchedule",
				Scheduler::DEFAULT_PRIORITY, schedule);
		m_next_schedule = schedule;
	} else {
		DEBUG_INFO("ArgosScheduler: not rescheduling");
	}

	// Perform any required RX scheduling
	rx_reschedule();
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

uint64_t ArgosScheduler::next_duty_cycle(unsigned int duty_cycle)
{
	// Do not proceed unless duty cycle is non-zero to save time
	if (duty_cycle == 0)
	{
		DEBUG_INFO("ArgosScheduler::next_duty_cycle: no schedule found as duty cycle is zero!");
		m_tr_nom_schedule = INVALID_SCHEDULE;
		return INVALID_SCHEDULE;
	}

	// Find epoch time for start of the "current" day
	uint64_t now = rtc->gettime();

	// If we have already a future schedule then we might not need to recompute
	if (m_tr_nom_schedule != INVALID_SCHEDULE && m_tr_nom_schedule >= (now * MS_PER_SEC))
	{
		// Make sure the TR_NOM is clear of the last transmission time
		if (m_last_transmission_schedule == INVALID_SCHEDULE ||
			(m_last_transmission_schedule != INVALID_SCHEDULE &&
			 (m_tr_nom_schedule / MS_PER_SEC) > m_last_transmission_schedule))
		{

			// If transmission was deferred than the schedule was cancelled and we need to reschedule using the existing TR_NOM
			if (m_is_deferred) {
				DEBUG_TRACE("ArgosScheduler::next_duty_cycle: use existing schedule [deferred]: in %.3f seconds", (double)(m_tr_nom_schedule - (now * MS_PER_SEC)) / MS_PER_SEC);
				m_is_deferred = false;
				return m_tr_nom_schedule - (now * MS_PER_SEC);
			}

			// Existing schedule still pending, don't reschedule
			DEBUG_TRACE("ArgosScheduler::next_duty_cycle: use existing schedule: in %.3f seconds", (double)(m_tr_nom_schedule - (now * MS_PER_SEC)) / MS_PER_SEC);
			return INVALID_SCHEDULE;
		}
	}

	// Handle the case where we have no existing schedule but the previous schedule was deferred meaning we might need
	// to transmit immediately owing to SWS going inactive
	if (m_is_deferred && m_earliest_tx != INVALID_SCHEDULE && m_earliest_tx > now && is_in_duty_cycle(m_earliest_tx * MS_PER_SEC, duty_cycle))
	{
		DEBUG_TRACE("ArgosScheduler::next_duty_cycle: using earliest TX in %llu secs", m_earliest_tx - now);
		m_is_deferred = false;
		return (m_earliest_tx - now) * MS_PER_SEC;
	}

	// A new TR_NOM schedule is required

	// Set schedule based on last TR_NOM point (if there is one)
	if (m_tr_nom_schedule == INVALID_SCHEDULE) {
		// Use start of day as the initial TR_NOM -- we don't allow
		// a -ve jitter amount in this case to avoid a potential -ve overflow
		uint64_t start_of_day = (now / SECONDS_PER_DAY) * SECONDS_PER_DAY * MS_PER_SEC;
		update_tx_jitter(0, TX_JITTER_MS);
		m_tr_nom_schedule = start_of_day + m_tx_jitter;
	}
	else
	{
		// Advance by TR_NOM + TX jitter if we have a previous TR_NOM
		// It should be safe to allow a -ve jitter because TR_NOM is always larger
		// than the jitter amount
		update_tx_jitter(-TX_JITTER_MS, TX_JITTER_MS);
		m_tr_nom_schedule += (m_argos_config.tr_nom * MS_PER_SEC) + m_tx_jitter;
	}

	DEBUG_TRACE("ArgosScheduler::next_duty_cycle: starting m_tr_nom_schedule = %.3f", (double)m_tr_nom_schedule / MS_PER_SEC);

	// We iterate forwards from the candidate m_tr_nom_schedule until we find a TR_NOM that
	// falls inside a permitted hour of transmission.  The maximum span we search is 24 hours.
	uint64_t elapsed_time = 0;
	while (elapsed_time <= (MS_PER_SEC * SECONDS_PER_DAY)) {
		//DEBUG_TRACE("ArgosScheduler::next_duty_cycle: now: %lu candidate schedule: %.3f elapsed: %.3f", now, (double)m_tr_nom_schedule / MS_PER_SEC, (double)elapsed_time / MS_PER_SEC);
		if (is_in_duty_cycle(m_tr_nom_schedule, duty_cycle) && m_tr_nom_schedule >= (now * MS_PER_SEC)) {
			DEBUG_TRACE("ArgosScheduler::next_duty_cycle: found schedule in %.3f seconds", (double)(m_tr_nom_schedule - (now * MS_PER_SEC)) / MS_PER_SEC);
			m_next_mode = ArgosMode::ARGOS_2;
			return (m_tr_nom_schedule - (now * MS_PER_SEC));
		} else {
			uint64_t delta;
			update_tx_jitter(-TX_JITTER_MS, TX_JITTER_MS);
			delta = (m_argos_config.tr_nom * MS_PER_SEC);
			delta += m_tx_jitter;
			m_tr_nom_schedule += delta;
			elapsed_time += delta;
		}
	}

	DEBUG_ERROR("ArgosScheduler::next_duty_cycle: no schedule found!");

	m_tr_nom_schedule = INVALID_SCHEDULE;

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

	// If we have an existing prepass schedule and we were interrupted by saltwater switch, then check
	// to see if the current prepass schedule is still valid to use.  We allow up to ARGOS_TX_MARGIN_SECS
	// before the end of the window
	if (m_next_prepass != INVALID_SCHEDULE &&
		m_is_deferred &&
		m_earliest_tx != INVALID_SCHEDULE &&
		m_earliest_tx >= curr_time &&
		m_earliest_tx < (m_next_prepass + m_prepass_duration - ARGOS_TX_MARGIN_SECS))
	{
		uint64_t earliest_tx = (std::max(m_earliest_tx, m_next_prepass) - curr_time) * MS_PER_SEC;
		DEBUG_TRACE("ArgosScheduler::next_prepass: rescheduling in %llu secs after SWS cleared", earliest_tx);
		m_is_deferred = false;
		return earliest_tx;
	}

	if (m_next_prepass != INVALID_SCHEDULE &&
		curr_time < (m_next_prepass + m_prepass_duration - ARGOS_TX_MARGIN_SECS)) {
		DEBUG_WARN("ArgosScheduler::next_prepass: existing prepass schedule in %llu secs is still valid", m_next_prepass - curr_time);
		return INVALID_SCHEDULE;
	}

	// Ensure start window is advanced from the previous transmission by the TR_NOM (repetition period)
	std::time_t start_time;
	if (m_last_transmission_schedule != INVALID_SCHEDULE)
		start_time = std::max(curr_time, (m_last_transmission_schedule + m_argos_config.tr_nom - ARGOS_TX_MARGIN_SECS));
	else
		start_time = curr_time;

	// Compute start and end time of the prepass search - we use a 24 hour window
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

	while (PREVIPASS_compute_next_pass(
    		&config,
			pass_predict.records,
			pass_predict.num_records,
			&next_pass)) {

		// Computed schedule is advanced by ARGOS_TX_MARGIN_SECS so that Artic R2 programming delay is not included in the window
		uint64_t now = rtc->gettime();
		uint64_t schedule;

		// Are we past the start point of the prepass window?
		if ((uint64_t)(next_pass.epoch - ARGOS_TX_MARGIN_SECS) < now) {
			// Ensure the schedule is at least TR_NOM away from previous transmission
			if (m_last_transmission_schedule != INVALID_SCHEDULE) {
				// If there is a previous transmission then advance TR_NOM with +/- jitter
				update_tx_jitter(-TX_JITTER_MS, TX_JITTER_MS);
				schedule = std::max(now, (m_last_transmission_schedule + (m_argos_config.tr_nom - ARGOS_TX_MARGIN_SECS))) * MS_PER_SEC;
				schedule += m_tx_jitter;
			} else {
				// This is the first transmission and we are inside the prepass window already, so schedule immediately
				schedule = now * MS_PER_SEC;
			}
		} else {
			// Current time is before the prepass window, so set our schedule to the start
			// of the window plus some +ve jitter amount only to avoid transmitting before
			// the window starts
			schedule = ((std::time_t)next_pass.epoch - ARGOS_TX_MARGIN_SECS);

			// But also make sure if there is a previous transmission we advance TR_NOM away from that
			if (m_last_transmission_schedule != INVALID_SCHEDULE)
				schedule = std::max(schedule, (m_last_transmission_schedule + (m_argos_config.tr_nom - ARGOS_TX_MARGIN_SECS)));

			// Apply +ve jitter
			update_tx_jitter(0, TX_JITTER_MS);
			schedule *= MS_PER_SEC;
			schedule += m_tx_jitter;
		}

		DEBUG_INFO("ArgosScheduler::next_prepass: sat=%u dl=%u ul=%u l=%llu t=%llu [%u,%.3f,%u]",
				(unsigned int)next_pass.satHexId, (unsigned int)next_pass.downlinkStatus, (unsigned int)next_pass.uplinkStatus,
					(m_last_transmission_schedule == INVALID_SCHEDULE) ? 0 :
							m_last_transmission_schedule, curr_time, (unsigned int)next_pass.epoch,
							(double)schedule / MS_PER_SEC, (unsigned int)next_pass.epoch + next_pass.duration);

		// Check we don't transmit off the end of the prepass window
		if ((schedule + (ARGOS_TX_MARGIN_SECS * MS_PER_SEC)) < ((uint64_t)next_pass.epoch + next_pass.duration) * MS_PER_SEC) {
			// We're good to go for this schedule, compute relative delay until the epoch arrives
			// and set the required Argos transmission mode
			DEBUG_INFO("ArgosScheduler::next_prepass: scheduled for %.3f seconds from now", (double)(schedule - (now * MS_PER_SEC)) / MS_PER_SEC);
			m_next_prepass = (schedule / MS_PER_SEC);
			m_next_mode = next_pass.uplinkStatus >= SAT_UPLK_ON_WITH_A3 ? ArgosMode::ARGOS_3 : ArgosMode::ARGOS_2;
			m_prepass_duration = next_pass.duration;

			// Do not update downlink status unless the existing prepass window has elapsed which is
			// signified by m_downlink_end = INVALID_SCHEDULE.
			if (m_downlink_end == INVALID_SCHEDULE && next_pass.downlinkStatus != SAT_DNLK_OFF) {
				// Set new downlink window end point
				m_downlink_end = (uint64_t)next_pass.epoch + next_pass.duration;
				m_downlink_start = (uint64_t)next_pass.epoch;
				DEBUG_TRACE("next_prepass: new DL RX window = [%llu, %llu]", m_downlink_start, m_downlink_end);
			}
			return schedule - (now * MS_PER_SEC);
		} else {
			DEBUG_TRACE("ArgosScheduler::next_prepass: computed schedule is too late for this window", next_pass.epoch, next_pass.duration);
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
		if (m_argos_config.time_sync_burst_en && !m_time_sync_burst_sent && m_num_gps_entries && rtc->is_set()) {
			prepare_time_sync_burst();
		} else {
			prepare_normal_burst();
		}
	} else {
		DEBUG_TRACE("ArgosScheduler::process_schedule: sws=%u t=%llu earliest_tx=%llu deferring transmission", m_switch_state, now, m_earliest_tx);
		m_is_deferred = true;
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
			m_gps_entry_burst_counter[m_num_gps_entries] = (m_argos_config.ntry_per_message == 0) ? UINT_MAX : m_argos_config.ntry_per_message;

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
	DEBUG_TRACE("ArgosScheduler::build_short_packet: day=%u", (unsigned int)gps_entry.info.day);
	PACK_BITS(gps_entry.info.hour, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: hour=%u", (unsigned int)gps_entry.info.hour);
	PACK_BITS(gps_entry.info.min, packet, base_pos, 6);
	DEBUG_TRACE("ArgosScheduler::build_short_packet: min=%u", (unsigned int)gps_entry.info.min);

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

void ArgosScheduler::adjust_logtime_for_gps_ontime(GPSLogEntry const& a, uint8_t& day, uint8_t& hour, uint8_t& minute)
{
	uint16_t year;
	uint8_t  month;
	uint8_t  second;
	std::time_t t;
	t = convert_epochtime(a.header.year, a.header.month, a.header.day, a.header.hours, a.header.minutes, a.header.seconds);
	t -= ((unsigned int)a.info.onTime / 1000);
	convert_datetime_to_epoch(t, year, month, day, hour, minute, second);
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

	// This will adjust the log time for the GPS on time since we want the time
	// of when the GPS was scheduled and not the log time
	uint8_t day, hour, minute;
	adjust_logtime_for_gps_ontime(gps_entries[0], day, hour, minute);

	PACK_BITS(day, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: day=%u", (unsigned int)day);
	PACK_BITS(hour, packet, base_pos, 5);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: hour=%u", (unsigned int)hour);
	PACK_BITS(minute, packet, base_pos, 6);
	DEBUG_TRACE("ArgosScheduler::build_long_packet: min=%u", (unsigned int)minute);

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
	DEBUG_INFO("ArgosScheduler::handle_packet: battery=%.2lfV freq=%lf power=%s mode=%s",
			   (double)battery_monitor->get_voltage() / 1000, m_argos_config.frequency, argos_power_to_string(m_argos_config.power), argos_mode_to_string(mode));
	DEBUG_INFO("ArgosScheduler::handle_packet: data=%s", Binascii::hexlify(packet).c_str());

	// Store parameters for deferred processing
	m_packet = packet;
	m_total_bits = total_bits;
	m_mode = mode;

	// Abort any scheduled RX to avoid race conditions
	rx_deschedule();

	// NOTE: Always power off before transmitting since the transceiver doesn't seem to like doing
	// a TX if RX mode has already been activated
	if (is_powered_on())
		power_off();

	// Power on and then handle the remainder of the transmission in the async event handler
	power_on([this](ArgosAsyncEvent e) { handle_tx_event(e); });
}

void ArgosScheduler::prepare_time_sync_burst() {

	DEBUG_TRACE("ArgosScheduler::prepare_time_sync_burst: num_gps_entries=%u", m_num_gps_entries);

	// Mark the time sync burst as sent because otherwise we would enter an infinite rescheduling scenario
	m_time_sync_burst_sent = true;

	if (m_num_gps_entries) {
		ArgosPacket packet;
		unsigned int index = m_num_gps_entries - 1;

		build_short_packet(m_gps_log_entry.at(index), packet);
		handle_packet(packet, SHORT_PACKET_BYTES * BITS_PER_BYTE, ArgosMode::ARGOS_2);

	} else {
		DEBUG_ERROR("ArgosScheduler::prepare_time_sync_burst: sensor log state is invalid, can't transmit");
	}
}

void ArgosScheduler::prepare_normal_burst() {
	unsigned int max_index = (((unsigned int)m_argos_config.depth_pile + MAX_GPS_ENTRIES_IN_PACKET-1) / MAX_GPS_ENTRIES_IN_PACKET);
	unsigned int index = m_msg_index % max_index;
	unsigned int span = std::min((unsigned int)MAX_GPS_ENTRIES_IN_PACKET, (unsigned int)m_argos_config.depth_pile);
	unsigned int eligible_gps_count = 0;
	unsigned int first_eligible_gps_index = -1;
	unsigned int max_msg_index;
	ArgosPacket packet;

	// Get latest configuration
	configuration_store->get_argos_configuration(m_argos_config);

	DEBUG_TRACE("ArgosScheduler::prepare_normal_burst: msg_index=%u/%u span=%u num_log_entries=%u", m_msg_index % max_index, max_index, span, m_num_gps_entries);

	// Mark last schedule attempt
	m_last_transmission_schedule = (m_tr_nom_schedule / MS_PER_SEC);

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
		handle_packet(packet, SHORT_PACKET_BYTES * BITS_PER_BYTE, m_next_mode);
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
		build_long_packet(gps_entries, packet);
		handle_packet(packet, LONG_PACKET_BYTES * BITS_PER_BYTE, m_next_mode);
	}

	// Increment for next message slot index
	m_msg_index++;

	// Prepass specific cleanup
	if (m_argos_config.mode == BaseArgosMode::PASS_PREDICTION) {
		m_last_transmission_schedule = rtc->gettime();
		m_next_prepass = INVALID_SCHEDULE;
	}
}

void ArgosScheduler::start(std::function<void(ServiceEvent&)> data_notification_callback) {

	DEBUG_INFO("ArgosScheduler::start");

	m_data_notification_callback = data_notification_callback;
	m_is_running = true;
	m_is_deferred = false;
	m_time_sync_burst_sent = false;
	m_msg_index = 0;
	m_num_gps_entries = 0;
	m_downlink_start = INVALID_SCHEDULE;
	m_next_prepass = INVALID_SCHEDULE;
	m_downlink_end = INVALID_SCHEDULE;
	m_tr_nom_schedule = INVALID_SCHEDULE;
	m_last_transmission_schedule = INVALID_SCHEDULE;
	m_gps_entry_burst_counter.clear();
	m_gps_log_entry.clear();
	configuration_store->get_argos_configuration(m_argos_config);

	// Generate TX jitter value
	m_rng = new std::mt19937(m_argos_config.argos_id);
	m_tx_jitter = 0;
}

void ArgosScheduler::stop() {
	DEBUG_INFO("ArgosScheduler::stop");
	deschedule();
	rx_deschedule();
	m_is_running = false;
	power_off();
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
			m_is_deferred = true;
		}
	}
}

void ArgosScheduler::handle_tx_event(ArgosAsyncEvent event) {
	switch (event) {
	case ArgosAsyncEvent::DEVICE_READY:
		DEBUG_TRACE("ArgosScheduler::handle_tx_event: DEVICE_READY");
		set_frequency(m_argos_config.frequency);
		set_tx_power(m_argos_config.power);
		if (m_data_notification_callback) {
	    	ServiceEvent e;
	    	e.event_type = ServiceEventType::ARGOS_TX_START;
	    	e.event_data = false;
	        m_data_notification_callback(e);
		}

		// This will generate TX_DONE event when finished
		send_packet(m_packet, m_total_bits, m_mode);
		break;

	case ArgosAsyncEvent::TX_DONE:
	{
		DEBUG_TRACE("ArgosScheduler::handle_tx_event: TX_DONE");
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
		reschedule();
		break;
	}

	case ArgosAsyncEvent::RX_PACKET:
		DEBUG_TRACE("ArgosScheduler::handle_tx_event: RX_PACKET");
		// TODO: handle RX packet
		break;

	case ArgosAsyncEvent::RX_TIMEOUT:
		DEBUG_TRACE("ArgosScheduler::handle_tx_event: RX_TIMEOUT");
		m_downlink_end = INVALID_SCHEDULE;
		if (is_powered_on()) {
			power_off();
		}
		break;

	case ArgosAsyncEvent::ERROR:
		DEBUG_ERROR("ArgosScheduler::handle_event: ERROR");
		power_off();
		break;

	default:
		break;
	}
}

void ArgosScheduler::handle_rx_event(ArgosAsyncEvent event) {
	switch (event) {
	case ArgosAsyncEvent::DEVICE_READY:
	{
		DEBUG_TRACE("ArgosScheduler::handle_rx_event: DEVICE_READY");
		set_frequency(m_argos_config.frequency);
		std::time_t now = rtc->gettime();
		// Enable RX mode with timeout which will trigger RX_TIMEOUT event
		unsigned int duration_sec = std::min((unsigned int)(m_downlink_end - now), m_argos_config.argos_rx_max_window);
		set_rx_mode(ArgosMode::ARGOS_3, MS_PER_SEC * duration_sec);
		break;
	}

	case ArgosAsyncEvent::TX_DONE:
	{
		DEBUG_TRACE("ArgosScheduler::handle_rx_event: TX_DONE: bad context!!!");
		break;
	}

	case ArgosAsyncEvent::RX_PACKET:
		DEBUG_TRACE("ArgosScheduler::handle_event: RX_PACKET");
		// TODO: handle RX packet
		break;

	case ArgosAsyncEvent::RX_TIMEOUT:
		DEBUG_TRACE("ArgosScheduler::handle_event: RX_TIMEOUT");
		m_downlink_end = INVALID_SCHEDULE;
		if (is_powered_on()) {
			power_off();
		}
		break;

	case ArgosAsyncEvent::ERROR:
		DEBUG_ERROR("ArgosScheduler::handle_event: ERROR");
		power_off();
		break;

	default:
		break;
	}
}
uint64_t ArgosScheduler::get_next_schedule() {
	return m_next_schedule;
}
