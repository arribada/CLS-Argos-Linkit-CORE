#pragma once
#include <functional>

class NrfIRQManager;

class NrfIRQ {
private:
	int m_pin;
	std::function<void()> m_func;

	void process_event();

public:
	NrfIRQ(int pin);
	~NrfIRQ();
	void enable(std::function<void()> func);
	void disable();
	bool poll();

	friend class NrfIRQManager;
};
