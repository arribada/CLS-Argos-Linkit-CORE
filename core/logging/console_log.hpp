#ifndef __CONSOLE_LOG_HPP_
#define __CONSOLE_LOG_HPP_

#include "logger.hpp"
#include "messages.hpp"

using namespace std;


class ConsoleLog : public Logger {

public:
	ConsoleLog() : Logger("Console") {}

private:
	void debug_formatter(const char *level, LogHeader *header, const char *msg) {
#ifndef CPPUTEST
		if (__get_IPSR()) // Indicate if this was created from an IRQ
			printf("%02u/%02u/%04u %02u:%02u:%02u [%s] IRQ\t%s\r\n", header->day, header->month, header->year, header->hours, header->minutes, header->seconds, level, msg);
		else
#endif
			printf("%02u/%02u/%04u %02u:%02u:%02u [%s]\t%s\r\n", header->day, header->month, header->year, header->hours, header->minutes, header->seconds, level, msg);
	}
	void gps_formatter(const GPSLogEntry *entry) {
		const char *name = log_type_name[entry->header.log_type];
		printf("[%s]\tlat: %lf lon: %lf\r\n", name, entry->info.lat, entry->info.lon);
	}

	void cam_formatter(const CAMLogEntry *entry) {
		const char *name = log_type_name[entry->header.log_type];
		printf("[%s]\tstate: %u counter: %u batt_voltage: %f\r\n", name, entry->info.event_type, entry->info.counter, entry->info.batt_voltage);
	}
	void startup_formatter(const SystemStartupLogEntry *entry) {
		const char *name = log_type_name[entry->header.log_type];
		printf("[%s]\tstartup_cause: %d", name, static_cast<int>(entry->cause));
	}
	void artic_formatter(const ArticTransceiverLogEntry *entry) {
		const char *name = log_type_name[entry->header.log_type];
		printf("[%s]\tartic_event: %d", name, static_cast<int>(entry->event));
		if (entry->event == ArticTransceiverEvent::TX)
			printf(" mode: %d msg_index: %lu tx_counter: %lu burst_counter: %lu tx_power: %d paylooad_type: %d",
					static_cast<int>(entry->mode), entry->msg_index, entry->tx_counter,
					entry->burst_counter, static_cast<int>(entry->tx_power), static_cast<int>(entry->payload_type));
		printf("\r\n");
	}
	void underwater_formatter(const UnderwaterLogEntry *entry) {
		const char *name = log_type_name[entry->header.log_type];
		printf("[%s]\tunderwater_state: %d\r\n", name, static_cast<int>(entry->event));
	}
	void battery_formatter(const BatteryLogEntry *entry) {
		const char *name = log_type_name[entry->header.log_type];
		printf("[%s]\tbatt_event: %d v: %u mV\r\n", name, static_cast<int>(entry->event), entry->voltage);
	}
	void zone_formatter(const ZoneLogEntry *entry) {
		const char *name = log_type_name[entry->header.log_type];
		printf("[%s]\tzone_event: %d zone_id: %u\r\n", name, static_cast<int>(entry->event), entry->zone_id);
	}
	void ota_update_formatter(const OTAFWUpdateLogEntry *entry) {
		const char *name = log_type_name[entry->header.log_type];
		printf("[%s]\tota_event: %d file_id: %u\r\n", name, static_cast<int>(entry->event), entry->file_id);
	}
	void ble_formatter(const BLEActionLogEntry *entry) {
		const char *name = log_type_name[entry->header.log_type];
		printf("[%s]\tble_state: %d cause: %d\r\n", name, static_cast<int>(entry->event), static_cast<int>(entry->cause));
	}
	void state_formatter(const StateChangeLogEntry *entry) {
		const char *name = log_type_name[entry->header.log_type];
		printf("[%s]\tnew_state: %d\r\n", name, static_cast<int>(entry->event));
	}

public:
	void create() override {}
	void truncate() override {}
	bool is_ready() override { return true; }
	unsigned int num_entries() override {return 0;}
	void read(void *, int) override { }
	void write(void *entry) override {
		LogEntry *p = (LogEntry *)entry;
		switch (p->header.log_type) {
		case LOG_ERROR:
		case LOG_WARN:
		case LOG_INFO:
		case LOG_TRACE:
			debug_formatter(log_type_name[p->header.log_type], &p->header, (const char * )p->data);
			break;
		case LOG_GPS:
			gps_formatter((const GPSLogEntry *)entry);
			break;
		case LOG_CAM:
			cam_formatter((const CAMLogEntry *)entry);
			break;
		case LOG_STATE:
			state_formatter((const StateChangeLogEntry *)entry);
			break;
		case LOG_BATTERY:
			battery_formatter((const BatteryLogEntry *)entry);
			break;
		case LOG_ARTIC:
			artic_formatter((const ArticTransceiverLogEntry *)entry);
			break;
		case LOG_STARTUP:
			startup_formatter((const SystemStartupLogEntry *)entry);
			break;
		case LOG_ZONE:
			zone_formatter((const ZoneLogEntry *)entry);
			break;
		case LOG_UNDERWATER:
			underwater_formatter((const UnderwaterLogEntry *)entry);
			break;
		case LOG_BLE:
			ble_formatter((const BLEActionLogEntry *)entry);
			break;
		case LOG_OTA_UPDATE:
			ota_update_formatter((const OTAFWUpdateLogEntry *)entry);
			break;
		default:
			// Not yet supported
			break;
		}
	}
};

#endif // __LOGGER_HPP_
