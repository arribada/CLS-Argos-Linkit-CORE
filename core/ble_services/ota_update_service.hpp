#ifndef __OTA_UPDATE_SERVICE_HPP_
#define __OTA_UPDATE_SERVICE_HPP_

#include "ble_service.hpp"


class OTAUpdateService : public BLEService {
public:
	static void start(void (*on_connected)(void), void (*on_disconnected)(void));
	static void stop();
};

#endif // __OTA_UPDATE_SERVICE_HPP_
