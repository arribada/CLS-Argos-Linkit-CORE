#ifndef __NRF_SWITCH_HPP_
#define __NRF_SWITCH_HPP_

#include "switch.hpp"

class NrfSwitchManager;

class NrfSwitch : public Switch {
private:
	void process_event(bool state);
public:
	NrfSwitch(int pin, unsigned int hystersis_time_ms);
	~NrfSwitch();
	void start(std::function<void(bool)> func) override;
	void stop() override;

	friend class NrfSwitchManager;
};

#endif // __NRF_SWITCH_HPP_
