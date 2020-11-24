#ifndef __BLE_SERVICE_HPP_
#define __BLE_SERVICE_HPP_

class BLEService {
public:
	static void start(void (*on_connected)(void), void (*on_disconnected)(void));
	static void stop();
};

#endif // __BLE_SERVICE_HPP_
