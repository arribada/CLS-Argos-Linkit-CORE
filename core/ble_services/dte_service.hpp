#ifndef __DTE_SERVICE_HPP_
#define __DTE_SERVICE_HPP_

#include "ble_service.hpp"


class DTEService : public BLEService {
public:
	static void start(void (*on_connected)(void), void (*on_disconnected)(void));
	static void stop();
};

#endif // __DTE_SERVICE_HPP_
