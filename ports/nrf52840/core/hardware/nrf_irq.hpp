#pragma once
#include <functional>

#include "scheduler.hpp"
#include "irq.hpp"

class NrfIRQManager;

class NrfIRQ : public IRQ {
private:
	int m_pin;
	Scheduler::TaskHandle m_task;

	void process_event();

public:
	NrfIRQ(int pin);
	~NrfIRQ();
	void enable(std::function<void()> func) override;
	void disable() override;
	bool poll();

	friend class NrfIRQManager;
};
