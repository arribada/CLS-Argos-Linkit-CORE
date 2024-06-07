#include "runcam.hpp"
#include "bsp.hpp"
#include "gpio.hpp"
#include "debug.hpp"

RunCam::RunCam() {
   DEBUG_TRACE("RunCam::RunCam() controlled by LDO"); 
   power_off();
}

RunCam::~RunCam() {
   power_off();
}

void RunCam::power_off()
{
   if (m_state == State::POWERED_OFF)
        return;
	DEBUG_TRACE("RunCam::power_off");
   m_state = State::POWERED_OFF;
	nrf_gpio_pin_clear(CAM_PWR_EN);
}

void RunCam::power_on()
{
   if (m_state == State::POWERED_ON)
        return;
	DEBUG_TRACE("RunCam::power_on");
	nrf_gpio_pin_set(CAM_PWR_EN);
}
