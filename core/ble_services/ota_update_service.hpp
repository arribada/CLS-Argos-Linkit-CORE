#ifndef __OTA_UPDATE_SERVICE_HPP_
#define __OTA_UPDATE_SERVICE_HPP_

#include "ble_service.hpp"


class OTAUpdateService : public BLEService {
	OTAUpdateService(void (*on_connected)(void), void (*on_disconnected)(void));
};

#endif // __OTA_UPDATE_SERVICE_HPP_
