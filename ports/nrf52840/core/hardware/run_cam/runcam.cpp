#include "runcam.hpp"
#include "bsp.hpp"
#include "gpio.hpp"
#include "debug.hpp"
#include "scheduler.hpp"

#include "pmu.hpp"

RunCam::RunCam() {
   DEBUG_TRACE("RunCam::RunCam() controlled by LDO"); 
   m_state = State::POWERED_OFF;
}

RunCam::~RunCam() {
   power_off();
}

void RunCam::power_off()
{
   if (m_state == State::POWERED_OFF)
         return;

	DEBUG_TRACE("RunCam::power_off");
   GPIOPins::set(CAM_PWR_BUTT);
   PMU::delay_ms(PWR_BUTT_DELAY);
   GPIOPins::clear(CAM_PWR_BUTT);
   PMU::delay_ms(PWR_DELAY);
   GPIOPins::clear(CAM_PWR_EN);
   m_state = State::POWERED_OFF;
   notify<CAMEventPowerOff>({});
}

void RunCam::power_on()
{
   if (m_state == State::POWERED_ON)
        return;
	DEBUG_TRACE("RunCam::power_on");
	//nrf_gpio_pin_set(CAM_PWR_EN);
   GPIOPins::set(CAM_PWR_EN);
   PMU::delay_ms(PWR_DELAY);
   GPIOPins::set(CAM_PWR_BUTT);
   PMU::delay_ms(PWR_BUTT_DELAY);
   GPIOPins::clear(CAM_PWR_BUTT);
   m_state = State::POWERED_ON;
   num_captures++;
   notify<CAMEventPowerOn>({});
}

bool RunCam::is_powered_on()
{
   if (m_state == State::POWERED_ON)
      return true;
   else
      return false;
}

unsigned int RunCam::get_num_captures()
{
   return num_captures;
}

