#ifndef _SWS_HPP__
#define _SWS_HPP__

#include <functional>
#include "scheduler.hpp"
#include "switch.hpp"


class SWS final : public Switch {
private:
	bool m_is_first_time;
	Scheduler::TaskHandle m_task_handle;
	unsigned int m_period_underwater_ms;
	unsigned int m_period_surface_ms;
	unsigned int m_sample_iteration;
	unsigned int m_sched_units;
	void sample_sws();

public:
	SWS(unsigned int sched_units=60) : Switch(0, 0) { m_is_first_time = true; m_sched_units=sched_units; }
	~SWS();
	void start(std::function<void(bool)> func) override;
	void stop() override;
};

#endif // _SWS_HPP__
