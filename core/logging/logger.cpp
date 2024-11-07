#include "debug.hpp"
#include "logger.hpp"
#include "rtc.hpp"
#include "timeutils.hpp"

extern RTC *rtc;


const char *LogFormatter::log_level_str(LogType t) {
		switch (t) {
		case LogType::LOG_ERROR:
			return "ERROR";
		case LogType::LOG_WARN:
			return "WARN";
		case LogType::LOG_INFO:
			return "INFO";
		case LogType::LOG_TRACE:
			return "TRACE";
		default:
		case LogType::LOG_GPS:
		case LogType::LOG_STARTUP:
		case LogType::LOG_ARTIC:
		case LogType::LOG_UNDERWATER:
		case LogType::LOG_BATTERY:
		case LogType::LOG_STATE:
		case LogType::LOG_ZONE:
		case LogType::LOG_OTA_UPDATE:
		case LogType::LOG_BLE:
			return "UNKNOWN";
		}
}

inline void Logger::sync_datetime(LogHeader &header) {
#ifdef DEBUG_USING_RTC
	if (rtc) {
		uint16_t year;
		convert_datetime_to_epoch(rtc->gettime(), year, header.month, header.day, header.hours, header.minutes, header.seconds);
		header.year = year;
	}
	else
#endif
	{
		header.year = header.month = header.day = header.hours = header.minutes = header.seconds = 0;
	}
}

Logger::Logger(const char *name) {
	m_log_formatter = nullptr;
	m_unique_id = LoggerManager::add(*this);
	m_name = name;
}

Logger::~Logger() {
	LoggerManager::remove(*this);
}

void Logger::set_log_level(int level) {
	m_log_level = level;
}

void Logger::set_log_formatter(LogFormatter* formatter) {
	m_log_formatter = formatter;
}

LogFormatter* Logger::get_log_formatter() {
	return m_log_formatter;
}

void Logger::warn(const char *msg, ...) {
	if (m_log_level >= LOG_LEVEL_WARN) {
		LogEntry buffer;
		va_list args;
		va_start(args, msg);
		vsnprintf(reinterpret_cast<char*>(buffer.data), sizeof(buffer.data), msg, args);
		va_end(args);
		buffer.header.log_type = LOG_WARN;
		buffer.header.payload_size = std::strlen(reinterpret_cast<char*>(buffer.data));
		sync_datetime(buffer.header);
		write(&buffer);
	}
}
void Logger::error(const char *msg, ...) {
	if (m_log_level >= LOG_LEVEL_ERROR) {
		LogEntry buffer;
		va_list args;
		va_start(args, msg);
		vsnprintf(reinterpret_cast<char*>(buffer.data), sizeof(buffer.data), msg, args);
		va_end(args);
		buffer.header.log_type = LOG_ERROR;
		buffer.header.payload_size = std::strlen(reinterpret_cast<char*>(buffer.data));
		sync_datetime(buffer.header);
		write(&buffer);
	}
}
void Logger::info(const char *msg, ...) {
	if (m_log_level >= LOG_LEVEL_INFO) {
		LogEntry buffer;
		va_list args;
		va_start(args, msg);
		vsnprintf(reinterpret_cast<char*>(buffer.data), sizeof(buffer.data), msg, args);
		va_end(args);
		buffer.header.log_type = LOG_INFO;
		buffer.header.payload_size = std::strlen(reinterpret_cast<char*>(buffer.data));
		sync_datetime(buffer.header);
		write(&buffer);
	}
}

void Logger::trace(const char *msg, ...) {
	if (m_log_level >= LOG_LEVEL_INFO) {
		LogEntry buffer;
		va_list args;
		va_start(args, msg);
		vsnprintf(reinterpret_cast<char*>(buffer.data), sizeof(buffer.data), msg, args);
		va_end(args);
		buffer.header.log_type = LOG_TRACE;
		buffer.header.payload_size = std::strlen(reinterpret_cast<char*>(buffer.data));
		sync_datetime(buffer.header);
		write(&buffer);
	}
}

void Logger::show_info() {
	DEBUG_INFO("Logger %s has %u entries", m_name, num_entries());
}

unsigned int Logger::get_unique_id() {
	return m_unique_id;
}

const char *Logger::get_name() {
	return m_name;
}

unsigned int LoggerManager::add(Logger& s) {
	m_map.insert({m_unique_identifier, s});
	return m_unique_identifier++;
}

void LoggerManager::remove(Logger& s) {
	m_map.erase(s.get_unique_id());
}

void LoggerManager::create() {
	for (auto const &s : m_map)
		s.second.create();
}

void LoggerManager::truncate()
{
	for (auto const &s : m_map)
		s.second.truncate();
}

void LoggerManager::show_info()
{
	for (auto const &s : m_map)
		s.second.show_info();
}

Logger *LoggerManager::find_by_name(const char *name) {
	for (auto const &s : m_map) {
		if (std::string(name) == std::string(s.second.get_name()))
			return &s.second;
	}

	return nullptr;
}
