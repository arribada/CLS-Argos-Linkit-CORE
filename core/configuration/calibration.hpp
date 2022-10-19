#pragma once

#include <string>
#include <map>
#include "filesystem.hpp"
#include "debug.hpp"

class Calibratable;


class CalibratableManager {
private:
	static inline std::map<std::string, Calibratable&> m_map;

public:
	static void add(Calibratable& s, const char *name);
	static void remove(Calibratable& s);
	static Calibratable &find_by_name(const char *name);
	static void save_all(bool force = false);
};

class Calibratable {
public:
	Calibratable(const char *name = "Calibratable") {
		CalibratableManager::add(*this, name);
	}
	virtual ~Calibratable() {
		CalibratableManager::remove(*this);
	}
	virtual void calibration_write(const double, const unsigned int) {};
	virtual void calibration_read(double &, const unsigned int) {};
	virtual void calibration_save(bool) {};
};

class Calibration {
public:
	Calibration(const char *name);
	~Calibration();
	double read(unsigned int offset);
	void write(unsigned int offset, double value);
	void reset();
	void save(bool force = false);

private:
	std::map<unsigned int, double> m_map;
	std::string m_filename;
	bool m_has_changed;

	void deserialize();
	void serialize();
};
