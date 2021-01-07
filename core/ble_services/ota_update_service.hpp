#ifndef __OTA_UPDATE_SERVICE_HPP_
#define __OTA_UPDATE_SERVICE_HPP_

#include "ble_service.hpp"


class OTAUpdateService : public BLEService {
public:
	void start(std::function<void()> const &on_connected, std::function<void()> const &on_disconnected, std::function<void()> const &on_received) override;
	void stop() override;
	void write(std::string str) override;
	std::string read_line() override;
};

#endif // __OTA_UPDATE_SERVICE_HPP_
