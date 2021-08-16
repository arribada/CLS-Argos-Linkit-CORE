#pragma once

#include "reed.hpp"
#include "switch.hpp"

class FakeReedSwitch : public ReedSwitch {

public:
	FakeReedSwitch(Switch&s) : ReedSwitch(s) {}
	void invoke_gesture(const ReedSwitchGesture g) {
		m_user_callback(g);
	}
};
