#include <iostream>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log_redirect.h"
#include "ble_interface.hpp"
#include "dte_protocol.hpp"
#include "bsp.hpp"

#define RED_LED_GPIO (NRF_GPIO_PIN_MAP(1, 7))

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

    nrf_log_redirect_init();

    BleInterface::get_instance().init();
    BleInterface::get_instance().advertising_start();

	printf("GenTracker Booted\r\n");

	nrf_gpio_cfg_output(RED_LED_GPIO);
	nrf_gpio_pin_clear(RED_LED_GPIO);

	for(;;)
	{
		nrf_gpio_pin_toggle(RED_LED_GPIO);
		nrf_delay_ms(500);

		auto req = BleInterface::get_instance().read_line();

		if (req.size())
		{
			std::string resp;
			auto action = DTEHandler::handle_dte_message(req, resp);

			std::cout << req;
			std::cout << resp << std::endl;
		}
		
	}

	return 0;
}