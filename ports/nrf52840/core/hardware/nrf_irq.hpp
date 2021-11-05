#pragma once
#include <functional>

#include "scheduler.hpp"

class NrfIRQManager;

class NrfIRQ {
private:
	int m_pin;
	std::function<void()> m_func;
	Scheduler::TaskHandle m_task;

	void process_event();

public:
	NrfIRQ(int pin);
	~NrfIRQ();
	void enable(std::function<void()> func);
	void disable();
	bool poll();

	friend class NrfIRQManager;
};
