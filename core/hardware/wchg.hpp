#pragma once
#include <string>

#include "error.hpp"

class WirelessCharger;

class WirelessChargerManager {
private:
	static inline WirelessCharger *m_instance = nullptr;
public:
	static void register_instance(WirelessCharger& instance) {
		m_instance = &instance;
	};
	static WirelessCharger& get_instance() {
		if (m_instance == nullptr)
			throw ErrorCode::RESOURCE_NOT_AVAILABLE;
		return *m_instance;
	}
};

class WirelessCharger {
public:
	WirelessCharger() {	WirelessChargerManager::register_instance(*this); }
	virtual ~WirelessCharger() {};
	virtual std::string get_chip_status() { return "UNKNOWN"; }
};
