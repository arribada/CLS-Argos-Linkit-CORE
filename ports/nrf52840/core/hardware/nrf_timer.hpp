#pragma once

#include <list>
#include <optional>
#include "timer.hpp"

class NrfTimer final : public Timer {
public:
	static NrfTimer& get_instance()
    {
        static NrfTimer instance;
        return instance;
    }

	void init();
	void uninit();

	uint64_t get_counter() override;
	TimerHandle add_schedule(std::function<void()> const &task_func, uint64_t target_count) override;
	void cancel_schedule(TimerHandle &handle) override;
	void start() override;
	void stop() override;

private:
	// Prevent copies to enforce this as a singleton
    NrfTimer() {}
    NrfTimer(NrfTimer const&)        = delete;
    void operator=(NrfTimer const&)  = delete;
};
