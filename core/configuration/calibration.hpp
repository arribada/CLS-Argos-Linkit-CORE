#pragma once

#include <string>
#include <map>
#include "filesystem.hpp"
#include "debug.hpp"

extern FileSystem *main_filesystem;

class Calibration {
public:
	Calibration(const char *name) : m_has_changed(false) {
		m_filename = std::string(name) + ".CAL";
		try {
			deserialize();
		} catch(...) {
			DEBUG_WARN("Calibration:: file %s missing or corrupted", m_filename.c_str());
		}
	}

	~Calibration() {
		save();
	}
	double read(unsigned int offset) {
		return m_map.at(offset);
	}
	void write(unsigned int offset, double value) {
		m_map.insert({offset, value});
		m_has_changed = true;
	}
	void reset() {
		m_map.clear();
		m_has_changed = true;
	}
	void save() {
		if (m_has_changed)
			serialize();
	}

private:
	std::map<unsigned int, double> m_map;
	std::string m_filename;
	bool m_has_changed;

	void deserialize() {
		LFSFile f(main_filesystem, m_filename.c_str(), LFS_O_RDONLY);
		while (1) {
			unsigned int offset;
			double value;
			if (f.read(&offset, sizeof(offset)) != sizeof(offset))
				break;
			if (f.read(&value, sizeof(value)) != sizeof(value))
				break;
			m_map.insert({offset, value});
		}
	}

	void serialize() {
		LFSFile f(main_filesystem, m_filename.c_str(), LFS_O_CREAT | LFS_O_WRONLY | LFS_O_TRUNC);
		for (auto const& entry : m_map) {
			f.write((void *)&entry.first, sizeof(entry.first));
			f.write((void *)&entry.second, sizeof(entry.second));
		}
	}
};
