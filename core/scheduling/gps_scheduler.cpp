#include <ctime>
#include "gps_scheduler.hpp"
#include "logger.hpp"
#include "rtc.hpp"
#include "timeutils.hpp"

extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;
extern Logger *sensor_log;
extern RTC *rtc;

#define MS_PER_SEC  (1000)
#define SEC_PER_MIN (60)

void GPSScheduler::start(std::function<void()> data_notification_callback)
{
    DEBUG_TRACE("GPSScheduler::start");
    configuration_store->get_gnss_configuration(m_gnss_config);

    m_data_notification_callback = data_notification_callback;
    m_gnss_data.pending_data_logging = false;
    m_gnss_data.pending_rtc_set = false;

    reschedule();
}

void GPSScheduler::stop()
{
    DEBUG_TRACE("GPSScheduler::stop");
    m_data_notification_callback = nullptr;
    deschedule();

    system_scheduler->cancel_task(m_task_update_rtc);
    system_scheduler->cancel_task(m_task_process_gnss_data);

    power_off();
}

void GPSScheduler::notify_saltwater_switch_state(bool state)
{
    DEBUG_TRACE("GPSScheduler::notify_saltwater_switch_state");
    (void) state; // Unused
}

void GPSScheduler::reschedule()
{
    if (!m_gnss_config.enable)
        return;

    std::time_t now = rtc->gettime();
    uint32_t aq_period = acquisition_period_to_seconds(m_gnss_config.dloc_arg_nom);

    // Find the next schedule time aligned to UTC 00:00
    std::time_t next_schedule = now - (now % aq_period) + aq_period;

    // Find the time in milliseconds until this schedule
    int64_t time_until_next_schedule_ms = (next_schedule - now) * MS_PER_SEC;

    DEBUG_TRACE("GPSScheduler::schedule_aquisition in %llu seconds", time_until_next_schedule_ms / 1000);

    deschedule(); // Ensure any previous schedule has been cleared
    m_task_acquisition_period = system_scheduler->post_task_prio(std::bind(&GPSScheduler::task_acquisition_period, this), Scheduler::DEFAULT_PRIORITY, time_until_next_schedule_ms);
}

void GPSScheduler::deschedule() {
	system_scheduler->cancel_task(m_task_acquisition_period);
    system_scheduler->cancel_task(m_task_acquisition_timeout);
}

uint32_t GPSScheduler::acquisition_period_to_seconds(BaseAqPeriod period) {

    switch (period)
    {
        case BaseAqPeriod::AQPERIOD_10_MINS:
            return 10 * SEC_PER_MIN;
        case BaseAqPeriod::AQPERIOD_15_MINS:
            return 15 * SEC_PER_MIN;
        case BaseAqPeriod::AQPERIOD_30_MINS:
            return 30 * SEC_PER_MIN;
        case BaseAqPeriod::AQPERIOD_60_MINS:
            return 60 * SEC_PER_MIN;
        case BaseAqPeriod::AQPERIOD_120_MINS:
            return 120 * SEC_PER_MIN;
        case BaseAqPeriod::AQPERIOD_360_MINS:
            return 360 * SEC_PER_MIN;
        case BaseAqPeriod::AQPERIOD_720_MINS:
            return 720 * SEC_PER_MIN;
        case BaseAqPeriod::AQPERIOD_1440_MINS:
            return 1440 * SEC_PER_MIN;
        default:
            DEBUG_ERROR("BaseAqPeriod value not recognised, defaulting to AQPERIOD_1440_MINS");
            return 1440 * SEC_PER_MIN;
    }
}

void GPSScheduler::task_acquisition_period() {
    DEBUG_TRACE("GPSScheduler::task_acquisition_period");
    try
    {
        power_on(std::bind(&GPSScheduler::gnss_data_callback, this, std::placeholders::_1));
    }
    catch(ErrorCode e)
    {
        // If our power on failed then log this as a failed GPS fix and notify the user
        log_invalid_gps_entry();
        reschedule();
        if (m_data_notification_callback)
            m_data_notification_callback();
        return;
    }

    m_task_acquisition_timeout = system_scheduler->post_task_prio(std::bind(&GPSScheduler::task_acquisition_timeout, this), Scheduler::DEFAULT_PRIORITY, m_gnss_config.acquisition_timeout * MS_PER_SEC);
}

void GPSScheduler::log_invalid_gps_entry()
{
    DEBUG_TRACE("GPSScheduler::log_invalid_gps_entry");
    GPSLogEntry gps_entry;
    gps_entry.header.log_type = LOG_GPS;

    populate_gps_log_with_time(gps_entry, rtc->gettime());

    gps_entry.valid = false;

    sensor_log->write(&gps_entry);
}

void GPSScheduler::task_acquisition_timeout() {
    DEBUG_TRACE("GPSScheduler::task_acquisition_timeout");
    power_off();

    log_invalid_gps_entry();

    reschedule();

    if (m_data_notification_callback)
        m_data_notification_callback();
}

void GPSScheduler::task_update_rtc()
{
    rtc->settime(convert_epochtime(m_gnss_data.data.year, m_gnss_data.data.month, m_gnss_data.data.day, m_gnss_data.data.hours, m_gnss_data.data.minutes, m_gnss_data.data.seconds));
    DEBUG_TRACE("GPSScheduler::task_update_rtc");
    m_gnss_data.pending_rtc_set = false;
}

void GPSScheduler::task_process_gnss_data()
{
    DEBUG_TRACE("GPSScheduler::task_process_gnss_data");

    GPSLogEntry gps_entry;
    gps_entry.header.log_type = LOG_GPS;

    populate_gps_log_with_time(gps_entry, rtc->gettime());

    // Store GPS data
    gps_entry.day = m_gnss_data.data.day;
    gps_entry.month = m_gnss_data.data.month;
    gps_entry.year = m_gnss_data.data.year;
    gps_entry.hour = m_gnss_data.data.hours;
    gps_entry.min = m_gnss_data.data.minutes;
    gps_entry.sec = m_gnss_data.data.seconds;
    gps_entry.lat = m_gnss_data.data.latitude;
    gps_entry.lon = m_gnss_data.data.longitude;
    gps_entry.valid = true;

    sensor_log->write(&gps_entry);

    power_off();

    reschedule();

    if (m_data_notification_callback)
        m_data_notification_callback();
    
    m_gnss_data.pending_data_logging = false;
}

void GPSScheduler::gnss_data_callback(GNSSData data) {
    // If we haven't finished processing our last data then ignore this one
    if (m_gnss_data.pending_data_logging || m_gnss_data.pending_rtc_set)
        return;
    
    m_gnss_data.data = data;
    m_gnss_data.pending_data_logging = true;
    m_gnss_data.pending_rtc_set = true;

    // Update our time based off this data, schedule this as high priority
    m_gnss_data.pending_rtc_set = true;
    m_task_update_rtc = system_scheduler->post_task_prio(std::bind(&GPSScheduler::task_update_rtc, this), Scheduler::HIGHEST_PRIORITY);

    // Don't process this data if it does not meet our required hdop threshold
    if (m_gnss_config.hdop_filter_enable &&
        m_gnss_data.data.hDOP > m_gnss_config.hdop_filter_threshold)
    {
        // We have a valid fix so there's no need to timeout anymore
        system_scheduler->cancel_task(m_task_acquisition_timeout);

        // Defer processing this data till we are outside of this interrupt context
        m_gnss_data.pending_data_logging = true;
        m_task_process_gnss_data = system_scheduler->post_task_prio(std::bind(&GPSScheduler::task_process_gnss_data, this), Scheduler::DEFAULT_PRIORITY);
    }
}

void GPSScheduler::populate_gps_log_with_time(GPSLogEntry &entry, std::time_t time)
{
    uint16_t year;
    uint8_t month, day, hour, min, sec;

    convert_datetime_to_epoch(time, year, month, day, hour, min, sec);

    entry.header.year = year;
    entry.header.month = month;
    entry.header.day = day;
    entry.header.hours = hour;
    entry.header.minutes = min;
    entry.header.seconds = sec;
}