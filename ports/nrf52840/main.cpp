#include "spi_flash_file_system.hpp"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "bsp.hpp"

#define BLOCK_COUNT   (4096)
#define BLOCK_SIZE    (4*1024)
#define PAGE_SIZE     (256)

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

	printf("GenTracker Booted\r\n");

	auto filesystem = LFSSpiFlashFileSystem(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);
	filesystem.init();

	if (filesystem.mount())
	{
		filesystem.format();
		filesystem.mount();
	}	

	nrf_gpio_cfg_output(RED_LED_GPIO);
	nrf_gpio_pin_clear(RED_LED_GPIO);

	for(;;)
	{
		nrf_gpio_pin_toggle(RED_LED_GPIO);
		nrf_delay_ms(500);
	}

	return 0;
}

/*
#include "scheduler.hpp"
#include "filesystem.hpp"
#include "gentracker.hpp"

#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)


// Global contexts
FileSystem *main_filesystem;

// FSM initial state -> BootState
FSM_INITIAL_STATE(GenTracker, BootState)

using fsm_handle = GenTracker;

int main() {
	// Setup global contexts
	// TODO: this should be replaced with a new FlashFileSystem variant
	main_filesystem = new RamFileSystem(BLOCK_COUNT, BLOCK_SIZE, PAGE_SIZE);

	// This will initialise the FSM
	fsm_handle::start();

	// The scheduler should run forever.  Any run-time exceptions should be handled by the
	// lambda function which passes them into the FSM
	Scheduler::run([](int err) {
		ErrorEvent e;
		e.error_code = static_cast<enum ErrorCode>(err);
		fsm_handle::dispatch(e);
	});

	return 0;
}
*/