#pragma once

#include <map>
#include <cstdint>
#include "base_types.hpp"
#include "events.hpp"

struct GPSNavSettings {
    BaseGNSSFixMode  fix_mode;
	BaseGNSSDynModel dyn_model;
	bool			 assistnow_autonomous_enable;
	bool             assistnow_offline_enable;
	bool             hdop_filter_en;
	unsigned int     hdop_filter_threshold;
	bool             hacc_filter_en;
    unsigned int     hacc_filter_threshold;
    bool             debug_enable = false;
    bool             sat_tracking = false;
    unsigned int     num_consecutive_fixes = 1;
    unsigned int     acquisition_timeout = 0;
};

struct GNSSData {
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
	uint8_t    fixType;
	uint8_t    flags;
	uint8_t    flags2;
	uint8_t    flags3;
	uint8_t    numSV;
	double     lon;       // Degrees
	double     lat;       // Degrees
	int32_t    height;    // mm
	int32_t    hMSL;      // mm
	uint32_t   hAcc;      // mm
	uint32_t   vAcc;      // mm
	int32_t    velN;      // mm
	int32_t    velE;      // mm
	int32_t    velD;      // mm
	int32_t    gSpeed;    // mm/s
	float      headMot;   // Degrees
	uint32_t   sAcc;      // mm/s
	float      headAcc;   // Degrees
	float      pDOP;
	float      vDOP;
	float      hDOP;
	float      headVeh;   // Degrees
	uint32_t   ttff;      // ms
};

struct GPSEventSignalAvailable {};
struct GPSEventError {};
struct GPSEventPowerOn {};
struct GPSEventPowerOff {
    bool fix_found;
    bool signal_found;
    GPSEventPowerOff(bool a, bool b) : fix_found(a), signal_found(b) {}
};
struct GPSEventPVT {
    GNSSData& data;
    GPSEventPVT(GNSSData& a) : data(a) {}
};

class GPSEventListener {
public:
    virtual ~GPSEventListener() {}
    virtual void react(const GPSEventPowerOn&) {}
    virtual void react(const GPSEventPowerOff&) {}
    virtual void react(const GPSEventError&) {}
    virtual void react(const GPSEventPVT&) {}
    virtual void react(const GPSEventSignalAvailable&) {}
};

class GPSDevice : public EventEmitter<GPSEventListener> {
public:
    virtual ~GPSDevice() {}
    // These methods are specific to the chipset and should be implemented by device-specific subclass
    virtual void power_off() = 0;
    virtual void power_on(const GPSNavSettings& nav_settings) = 0;
};
