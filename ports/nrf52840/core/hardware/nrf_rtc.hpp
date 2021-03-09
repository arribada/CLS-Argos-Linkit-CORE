#pragma once

#include "rtc.hpp"

class NrfRTC final : public RTC {
public:
	static NrfRTC& get_instance()
    {
        static NrfRTC instance;
        return instance;
    }

	void init();
	void uninit();

	std::time_t gettime() override;
	void settime(std::time_t time) override;
	bool is_set() override;

private:
	// Prevent copies to enforce this as a singleton
	bool m_is_set;
    NrfRTC() : m_is_set(false) {}
    NrfRTC(NrfRTC const&)          = delete;
    void operator=(NrfRTC const&)  = delete;

    int64_t getuptime();
};
