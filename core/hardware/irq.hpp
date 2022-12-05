#pragma once

#include <functional>


class IRQ {
protected:
	std::function<void()> m_func;

public:
	IRQ() : m_func(nullptr) {}
	virtual ~IRQ() {}
	virtual void enable(std::function<void()> func) {
		m_func = func;
	}
	virtual void disable() {
		m_func = nullptr;
	}
};
