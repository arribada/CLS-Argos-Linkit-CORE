#pragma once

#include "irq.hpp"

class FakeIRQ : public IRQ {
public:
	void invoke() {
		if (m_func)
			m_func();
	}
};
