#include <iostream>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log_redirect.h"
#include "ble_interface.hpp"
#include "dte_handler.hpp"
#include "is25_flash_file_system.hpp"
#include "nrf_memory_access.hpp"
#include "config_store_fs.hpp"
#include "debug.hpp"
#include "console_log.hpp"
#include "debug.hpp"
#include "bsp.hpp"

#define RED_LED_GPIO (NRF_GPIO_PIN_MAP(1, 7))

ConfigurationStore *configuration_store;
MemoryAccess *memory_access;
Logger *sensor_log;
Logger *system_log;
ConsoleLog *console_log;

// Redirect std::cout and printf output to debug UART
// We have to define this as extern "C" as we are overriding a weak C function
extern "C" int _write(int file, char *ptr, int len)
{
	nrfx_uarte_tx(&BSP::UART_Inits[BSP::UART_1].uarte, reinterpret_cast<const uint8_t *>(ptr), len);
	return len;
}

int main() {

	nrfx_uarte_init(&BSP::UART_Inits[BSP::UART_1].uarte, &BSP::UART_Inits[BSP::UART_1].config, nullptr);
    setvbuf(stdout, NULL, _IONBF, 0);

	ConsoleLog console_console_log;
	console_log = &console_console_log;

    nrf_log_redirect_init();
	
    BleInterface::get_instance().init();

	printf("GenTracker Booted\r\n");

	nrf_gpio_cfg_output(RED_LED_GPIO);
	nrf_gpio_pin_clear(RED_LED_GPIO);

	Is25FlashFileSystem lfs_file_system;

	lfs_file_system.init();

	if (lfs_file_system.mount())
	{
		lfs_file_system.format();
		lfs_file_system.mount();
	}

	LFSConfigurationStore store(lfs_file_system);
	configuration_store = &store;
	store.init();

	NrfMemoryAccess nrf_memory_access;
	memory_access = &nrf_memory_access;

	ConsoleLog console_system_log;
	system_log = &console_system_log;

	ConsoleLog console_sensor_log;
	sensor_log = &console_sensor_log;

    BleInterface::get_instance().advertising_start();

	for(;;)
	{
		nrf_gpio_pin_toggle(RED_LED_GPIO);

		auto req = BleInterface::get_instance().read_line();

		if (req.size())
		{
			DEBUG_TRACE("received: %s", req.c_str());

			std::string resp;
			auto action = DTEHandler::handle_dte_message(req, resp);
			(void) action;

			if (resp.size())
			{
				DEBUG_TRACE("responded: %s", resp.c_str());
				BleInterface::get_instance().write(resp);
			}			
		}
		
	}

	return 0;
}