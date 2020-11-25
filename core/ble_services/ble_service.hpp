#ifndef __BLE_SERVICE_HPP_
#define __BLE_SERVICE_HPP_

class BLEService {
public:
	virtual void start(void (*on_connected)(void), void (*on_disconnected)(void)) = 0;
	virtual void stop() = 0;
};

#endif // __BLE_SERVICE_HPP_
