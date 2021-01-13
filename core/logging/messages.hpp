#ifndef __MESSAGES_HPP_
#define __MESSAGES_HPP_

#include <stdint.h>
#include "base_types.hpp"

#define MAX_LOG_PAYLOAD    120

static constexpr const char *log_type_name[16] = {
	"GPS",
	"STARTUP",
	"ARTIC",
	"UNDERWATER",
	"BATTERY",
	"STATE",
	"ZONE",
	"OTA_UPDATE",
	"BLE",
	"ERROR",
	"WARN",
	"INFO",
	"TRACE"
};

enum LogType : uint8_t {
	LOG_GPS,
	LOG_STARTUP,
	LOG_ARTIC,
	LOG_UNDERWATER,
	LOG_BATTERY,
	LOG_STATE,
	LOG_ZONE,
	LOG_OTA_UPDATE,
	LOG_BLE,
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_TRACE
};

struct __attribute__((packed)) LogHeader {
	uint8_t  day;
	uint8_t  month;
	uint16_t year;
	uint8_t  hours;
	uint8_t  minutes;
	uint8_t  seconds;
	LogType  log_type;
};

struct LogEntry {
	LogHeader header;
	union {
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

enum class GPSEventType : uint8_t { ON, OFF, UPDATE, FIX, NO_FIX };
enum class GPSFixType : uint8_t { NO_FIX, DEAD_RECKONING, DIM_2D, DIM_3D };

struct __attribute__((packed)) GPSLogEntry {
	LogHeader header;
	union {
		struct {
			GPSEventType event_type;
			uint16_t   batt_voltage;
			uint32_t   iTOW;
			uint16_t   year;
			uint8_t    month;
			uint8_t    day;
			uint8_t    hour;
			uint8_t    min;
			uint8_t    sec;
			uint8_t    valid;
			uint32_t   tAcc;
			int32_t    nano;
			GPSFixType fixType;
			uint8_t    numSV;
			double     lon;
			double     lat;
			int32_t    height;
			int32_t    hMSL;
			uint32_t   hAcc;
			uint32_t   vAcc;
			int32_t    velN;
			int32_t    velE;
			int32_t    velD;
			int32_t    gSpeed;
			double     headMot;
			uint32_t   sAcc;
			uint32_t   headAcc;
			uint16_t   pDOP;
			uint8_t    flags3;
			uint8_t    reserved[5];
			int32_t    headVeh;
			int16_t    magDec;
			uint16_t   magAcc;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

enum class StartupCause : uint8_t { BROWNOUT, WATCHDOG, HARD_RESET, FACTORY_RESET, SOFT_RESET };

struct __attribute__((packed)) SystemStartupLogEntry {
	LogHeader header;
	union {
		struct {
			StartupCause cause;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};


enum class ArticTransceiverEvent : uint8_t { OFF, ON, TX };
enum class ArticPayloadType : uint8_t { SHORT, LONG };


struct __attribute__((packed)) ArticTransceiverLogEntry {
	LogHeader header;
	union {
		struct {
			ArticTransceiverEvent event;
			ArticPayloadType      payload_type;
			uint32_t			  msg_index;
			uint32_t			  tx_counter;
			BaseArgosPower		  tx_power;
			uint32_t			  burst_counter;
			BaseArgosMode		  mode;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

enum class UnderwaterEvent : uint8_t { DRY, WET };

struct __attribute__((packed)) UnderwaterLogEntry {
	LogHeader header;
	union {
		struct {
			UnderwaterEvent event;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

enum class BatteryEvent : uint8_t { VUPDATE, LOW_THRESHOLD, CHARGING_ON, CHARGING_OFF };

struct __attribute__((packed)) BatteryLogEntry {
	LogHeader header;
	union {
		struct {
			BatteryEvent event;
			uint16_t 	 voltage;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

enum class StateChangeEvent : uint8_t { BOOT, CONFIGURATION, OPERATIONAL };

struct __attribute__((packed)) StateChangeLogEntry {
	LogHeader header;
	union {
		struct {
			StateChangeEvent event;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

enum class ZoneEvent : uint8_t { WRITE, ENTER, EXIT };

struct __attribute__((packed)) ZoneLogEntry {
	LogHeader header;
	union {
		struct {
			ZoneEvent event;
			uint8_t   zone_id;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

enum class OTAFWUpdateEvent : uint8_t { START, SUCCESS, FAIL, ABORT };

struct __attribute__((packed)) OTAFWUpdateLogEntry {
	LogHeader header;
	union {
		struct {
			OTAFWUpdateEvent event;
			uint8_t          file_id;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

enum class BLEEvent : uint8_t { OFF, ON };
enum class BLEEventCause : uint8_t { REEDSWITCH, CLIENT, TIMEOUT };

struct __attribute__((packed)) BLEActionLogEntry {
	LogHeader header;
	union {
		struct {
			BLEEvent event;
			BLEEventCause cause;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};


#endif // __MESSAGES_HPP_

