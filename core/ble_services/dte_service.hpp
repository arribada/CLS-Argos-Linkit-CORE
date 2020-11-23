#ifndef __DTE_SERVICE_HPP_
#define __DTE_SERVICE_HPP_

#include "ble_service.hpp"


class DTEService : public BLEService {
	DTEService(void (*on_connected)(void), void (*on_disconnected)(void));
};

#endif // __DTE_SERVICE_HPP_
