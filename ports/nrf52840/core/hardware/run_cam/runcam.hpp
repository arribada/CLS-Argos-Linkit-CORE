#pragma once

#include <cstdint>
#include "cam.hpp"
#include "scheduler.hpp"

class RunCam: public CAMDevice {
public:
    RunCam();
    ~RunCam();
	void power_off() override;
	void power_on() override;
	bool is_powered_on() override;
	unsigned int get_num_captures() override;
private:
	enum class State
	{
		POWERED_OFF,
		POWERED_ON
	} m_state;

	unsigned int num_captures;

};