#pragma once

#include <cstdint>
#include "cam.hpp"

class RunCam: public CAMDevice {
public:
    RunCam();
    ~RunCam();
	void power_off() override;
	void power_off() override;
private:
	enum class State
	{
		POWERED_OFF,
		POWERED_ON
	} m_state;


};