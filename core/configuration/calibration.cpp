#include "calibration.hpp"
#include "filesystem.hpp"
#include "debug.hpp"
#include "error.hpp"

extern FileSystem *main_filesystem;

void CalibratableManager::add(Calibratable& s, const char *name) {
	if (m_map.count(std::string(name)))
		throw ErrorCode::KEY_ALREADY_EXISTS; // Don't allow duplicate keys
	m_map.insert({std::string(name), s});
}

void CalibratableManager::remove(Calibratable& s) {
	for (auto const &p : m_map) {
		if (&p.second == &s) {
			m_map.erase(p.first);
			return;
		}
	}
	throw ErrorCode::KEY_DOES_NOT_EXIST; // Don't allow a remove that doesn't exist
}

Calibratable &CalibratableManager::find_by_name(const char *name) {
	return m_map.at(std::string(name));
}

void CalibratableManager::save_all(bool force) {
	for (auto const &p : m_map) {
		p.second.calibration_save(force);
	}
}

Calibration::Calibration(const char *name) : m_has_changed(false) {
	m_filename = std::string(name) + ".CAL";
	try {
		deserialize();
	} catch(...) {
		DEBUG_WARN("Calibration:: file %s missing or corrupted", m_filename.c_str());
	}
}

Calibration::~Calibration() {
	save();
}

double Calibration::read(unsigned int offset) {
	return m_map.at(offset);
}

void Calibration::write(unsigned int offset, double value) {
	m_map[offset] = value;
	m_has_changed = true;
}

void Calibration::reset() {
	m_map.clear();
	m_has_changed = true;
}

void Calibration::save(bool force) {
	if (m_has_changed || force)
		serialize();
}

void Calibration::deserialize() {
	LFSFile f(main_filesystem, m_filename.c_str(), LFS_O_RDONLY);
	while (1) {
		unsigned int offset;
		double value;
		if (f.read(&offset, sizeof(offset)) != sizeof(offset))
			break;
		if (f.read(&value, sizeof(value)) != sizeof(value))
			break;
		m_map[offset] = value;
	}
}

void Calibration::serialize() {
	LFSFile f(main_filesystem, m_filename.c_str(), LFS_O_CREAT | LFS_O_WRONLY | LFS_O_TRUNC);
	for (auto const& entry : m_map) {
		f.write((void *)&entry.first, sizeof(entry.first));
		f.write((void *)&entry.second, sizeof(entry.second));
	}
}
