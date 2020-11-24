#ifndef __MESSAGES_HPP_
#define __MESSAGES_HPP_

#include <stdint.h>

#define MAX_LOG_PAYLOAD    120

static const char *log_type_name[8] = {
	"GPS",
	"ERROR",
	"WARN",
	"INFO",
	"TRACE"
};

enum LogType : uint8_t {
	LOG_GPS,
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

enum GPSEventType : uint8_t { ON, OFF, UPDATE, FIX, NO_FIX };
enum GPSFixType : uint8_t { NO_FIX, DEAD_RECKONING, DIM_2D, DIM_3D };

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
			int32_t    lon;
			int32_t    lat;
			int32_t    height;
			int32_t    hMSL;
			uint32_t   hAcc;
			uint32_t   vAcc;
			int32_t    velN;
			int32_t    velE;
			int32_t    velD;
			int32_t    gSpeed;
			int32_t    headMot;
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

#endif // __MESSAGES_HPP_

