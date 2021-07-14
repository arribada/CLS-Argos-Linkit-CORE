#include "ledsm.hpp"
#include "debug.hpp"

extern RGBLed *status_led;
extern Timer *system_timer;


void LEDOff::entry() {
	status_led->off();
}

void LEDBoot::entry() {
	status_led->set(RGBLedColor::WHITE);
}

void LEDPowerDown::entry() {
	status_led->flash(RGBLedColor::WHITE, 125);
}

void LEDIdleError::entry() {
	status_led->set(RGBLedColor::RED);
}

void LEDIdleBatteryNominal::entry() {
	status_led->set(RGBLedColor::GREEN);
}

void LEDIdleBatteryLow::entry() {
	status_led->set(RGBLedColor::YELLOW);
}

void LEDError::entry() {
	status_led->flash(RGBLedColor::RED);
}

void LEDPreOperationalBatteryNominal::entry() {
	status_led->flash(RGBLedColor::GREEN);
}

void LEDPreOperationalBatteryLow::entry() {
	status_led->flash(RGBLedColor::YELLOW);
}

void LEDConfigNotConnected::entry() {
	status_led->flash(RGBLedColor::BLUE);
}

void LEDConfigConnected::entry() {
	status_led->set(RGBLedColor::BLUE);
}

void LEDGNSSOn::entry() {
	m_is_gnss_on = true;
	status_led->flash(RGBLedColor::CYAN, 1000);
}

void LEDGNSSOffWithFix::entry() {
	m_is_gnss_on = false;
	status_led->set(RGBLedColor::GREEN);
	system_timer->add_schedule([this]() {
		transit<LEDOff>();
	}, system_timer->get_counter() + 3000);
}

void LEDGNSSOffWithoutFix::entry() {
	m_is_gnss_on = false;
	status_led->set(RGBLedColor::RED);
	system_timer->add_schedule([this]() {
		transit<LEDOff>();
	}, system_timer->get_counter() + 3000);
}

void LEDArgosTX::entry() {
	status_led->set(RGBLedColor::MAGENTA);
}

void LEDArgosTXComplete::entry() {
	system_timer->add_schedule([this]() {
		if (m_is_gnss_on)
			transit<LEDGNSSOn>();
		else
			transit<LEDOff>();
	}, system_timer->get_counter());
}

void LEDMenu::entry() {
	m_last_menu_state = next_menu_state(m_last_menu_state);
	RGBLedColor c;
	c = menu_color(m_last_menu_state);
	status_led->set(c);
	DEBUG_TRACE("entry: LEDMenu: menu_state=%u color=%u", (unsigned int)m_last_menu_state, (unsigned int)c);
	m_timer_handle = system_timer->add_schedule([this]() {
		entry();
	}, system_timer->get_counter() + 3000);
}

void LEDMenu::exit() {
	DEBUG_TRACE("exit: LEDMenu");
	system_timer->cancel_schedule(m_timer_handle);
	m_last_menu_state = LEDMenuState::INACTIVE;
}
