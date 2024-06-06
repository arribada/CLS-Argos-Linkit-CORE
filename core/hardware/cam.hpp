#pragma once

#include <map>
#include <cstdint>
#include "base_types.hpp"
#include "events.hpp"
/* 
struct CAMSettings {
	unsigned int time_on;
	unsigned int time_off;
} */
/* 
struct CAMData {
	bool state;
	uint16_t counter
};

 */
struct CAMEventError {};
struct CAMEventPowerOn {};
struct CAMEventPowerOff {
    bool fix_found;
    CAMEventPowerOff(bool a) : fix_found(a) {}
};

class CAMEventListener {
public:
    virtual ~CAMEventListener() {}
    virtual void react(const CAMEventPowerOn&) {}
    virtual void react(const CAMEventPowerOff&) {}
    virtual void react(const CAMEventError&) {}
};

class CAMDevice : public EventEmitter<CAMEventListener> {
public:
    virtual ~CAMDevice() {}
    // These methods are specific to the chipset and should be implemented by device-specific subclass
    virtual void power_off() = 0;
    //virtual void power_on(const CAMSettings& cam_settings) = 0;
    virtual void power_on() = 0;
};
