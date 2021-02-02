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
    configuration_store->get_gnss_configuration(m_gnss_config);
    m_data_notification_callback = data_notification_callback;

    reschedule();
}

void GPSScheduler::stop()
{
    m_data_notification_callback = nullptr;
    deschedule();
    power_off();
}

void GPSScheduler::notify_saltwater_switch_state(bool state)
{
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

    deschedule(); // Ensure any previous schedule has been cleared
    m_acquisition_period_task = system_scheduler->post_task_prio(std::bind(&GPSScheduler::acquisition_period_task, this), Scheduler::DEFAULT_PRIORITY, time_until_next_schedule_ms);
}

void GPSScheduler::deschedule() {
	system_scheduler->cancel_task(m_acquisition_period_task);
    system_scheduler->cancel_task(m_acquisition_timeout_task);
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

void GPSScheduler::acquisition_period_task() {
    power_on(std::bind(&GPSScheduler::gnss_data_callback, this, std::placeholders::_1));

    m_acquisition_period_task = system_scheduler->post_task_prio(std::bind(&GPSScheduler::acquisition_timeout_task, this), Scheduler::DEFAULT_PRIORITY, m_gnss_config.acquisition_timeout * MS_PER_SEC);
}

void GPSScheduler::acquisition_timeout_task() {
    power_off();

    // Log a dummy fix with valid = 0

    GPSLogEntry gps_entry;
    gps_entry.header.log_type = LOG_GPS;

    populate_gps_log_with_time(gps_entry, rtc->gettime());

    gps_entry.valid = false;

    sensor_log->write(&gps_entry);

    reschedule();

    if (m_data_notification_callback)
        m_data_notification_callback();
}

void GPSScheduler::gnss_data_callback(GNSSData data) {
    // Ignore this data if it does not meet our required hdop threshold
    if (m_gnss_config.hdop_filter_enable)
        if (data.hDOP > m_gnss_config.hdop_filter_threshold)
            return;

    GPSLogEntry gps_entry;
    gps_entry.header.log_type = LOG_GPS;

    populate_gps_log_with_time(gps_entry, rtc->gettime());

    // Store GPS data
    gps_entry.day = data.day;
    gps_entry.month = data.month;
    gps_entry.year = data.year;
    gps_entry.hour = data.hours;
    gps_entry.min = data.minutes;
    gps_entry.sec = data.seconds;
    gps_entry.lat = data.latitude;
    gps_entry.lon = data.longitude;
    gps_entry.valid = true;

    sensor_log->write(&gps_entry);

    power_off();
    system_scheduler->cancel_task(m_acquisition_timeout_task);

    reschedule();

    if (m_data_notification_callback)
        m_data_notification_callback();
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