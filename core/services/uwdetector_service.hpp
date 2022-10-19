#pragma once

#include <functional>
#include "scheduler.hpp"
#include "service.hpp"


class UWDetectorService : public Service {

public:
	UWDetectorService(const char *name = "UWDetector") :
		Service(ServiceIdentifier::UW_SENSOR, name)	{
		m_is_first_time = true;
		m_current_state = false;
	}
	virtual ~UWDetectorService() {}

protected:
	double m_activation_threshold;
	unsigned int m_enable_sample_delay;

private:
	bool m_is_first_time;
	bool m_current_state;
	bool m_pending_state;
	unsigned int m_period_underwater_ms;
	unsigned int m_period_surface_ms;
	unsigned int m_sample_iteration;
	unsigned int m_max_samples;
	unsigned int m_sample_gap;
	unsigned int m_min_dry_samples;
	unsigned int m_dry_count;

	virtual bool detector_state() { return false; };
	virtual bool service_is_enabled() = 0;
	void service_init() override;
	void service_term() override;
	unsigned int service_next_schedule_in_ms() override;
	void service_initiate() override;
	bool service_is_usable_underwater() override;
};
