#pragma once

#include <functional>
#include "scheduler.hpp"
#include "switch.hpp"


class UWDetector : public Switch {
private:
	bool m_is_first_time;
	Scheduler::TaskHandle m_task_handle;
	unsigned int m_period_underwater_ms;
	unsigned int m_period_surface_ms;
	unsigned int m_sample_iteration;
	unsigned int m_sched_units;
	void sample_detector();

public:
	UWDetector(unsigned int sched_units=1) : Switch(0, 0) { m_is_first_time = true; m_sched_units=sched_units; }
	~UWDetector();
	void start(std::function<void(bool)> func) override;
	void stop() override;
	virtual bool detector_state() { return false; };
};
