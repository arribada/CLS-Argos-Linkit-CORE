#ifndef __BLE_SERVICE_HPP_
#define __BLE_SERVICE_HPP_

#include <functional>
#include <string>

enum class BLEServiceEventType {
	CONNECTED,
	DISCONNECTED,
	DTE_DATA_RECEIVED,
	OTA_START,
	OTA_FILE_DATA,
	OTA_ABORT,
	OTA_END
};

struct BLEServiceEvent {
	BLEServiceEventType event_type;
	union {
		struct {
			unsigned int file_id;
			unsigned int file_size;
			unsigned int crc32;
		};
		struct {
			void *data;
			unsigned int length;
		};
	};
};

class BLEService {
public:
	virtual ~BLEService() {}
	virtual void start(std::function<int(BLEServiceEvent& event)> on_event) = 0;
	virtual void stop() = 0;
	virtual bool write(std::string str) = 0;
	virtual std::string read_line() = 0;
	virtual void set_device_name(const std::string&) = 0;
};

#endif // __BLE_SERVICE_HPP_
