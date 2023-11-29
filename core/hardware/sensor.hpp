#pragma once

#include <functional>
#include <map>
#include <string>

#include "calibration.hpp"
#include "error.hpp"

class Sensor;


class Sensor : public Calibratable {
public:
	Sensor(const char *name = "Sensor");
	virtual ~Sensor();
	virtual double read(unsigned int port = 0) = 0;
	virtual void install_event_handler(unsigned int, std::function<void()>) {}
	virtual void remove_event_handler(unsigned int) {}
};


class SensorManager {
private:
	static inline std::map<std::string, Sensor&> m_map;

public:
	static void add(Sensor& s, const char *name) {
		if (m_map.count(std::string(name)))
			throw ErrorCode::KEY_ALREADY_EXISTS; // Don't allow duplicate keys
		m_map.insert({std::string(name), s});
	}
	static void remove(Sensor& s) {
		for (auto const &p : m_map) {
			if (&p.second == &s) {
				m_map.erase(p.first);
				return;
			}
		}
		throw ErrorCode::KEY_DOES_NOT_EXIST; // Don't allow a remove that doesn't exist
	}
	static Sensor &find_by_name(const char *name) {
		return m_map.at(std::string(name));
	}
};
