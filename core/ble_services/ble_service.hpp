#ifndef __BLE_SERVICE_HPP_
#define __BLE_SERVICE_HPP_

#include <functional>
#include <string>

class BLEService {
public:
	virtual ~BLEService() {}
	virtual void start(std::function<void()> const &on_connected, std::function<void()> const &on_disconnected, std::function<void()> const &on_received) = 0;
	virtual void stop() = 0;
	virtual void write(std::string str) = 0;
	virtual std::string read_line() = 0;
};

#endif // __BLE_SERVICE_HPP_
