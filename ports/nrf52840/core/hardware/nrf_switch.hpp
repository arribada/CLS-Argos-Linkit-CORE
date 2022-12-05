#pragma once

#include "switch.hpp"
#include "timer.hpp"

class NrfSwitchManager;

class NrfSwitch : public Switch {
private:
	Timer::TimerHandle m_timer_handle;

	void process_event(bool state);
	void update_state(bool state);

public:
	NrfSwitch(int pin, unsigned int hystersis_time_ms, bool active_status = true);
	~NrfSwitch();
	void start(std::function<void(bool)> func) override;
	void stop() override;
	bool get_state();
	void pause() override;
	void resume() override;

	friend class NrfSwitchManager;
};
