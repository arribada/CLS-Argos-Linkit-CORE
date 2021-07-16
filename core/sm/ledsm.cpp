#include "ledsm.hpp"
#include "debug.hpp"

extern RGBLed *status_led;
extern Timer *system_timer;


void LEDOff::entry() {
	DEBUG_TRACE("LEDOff: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->off();
}

void LEDBoot::entry() {
	DEBUG_TRACE("LEDBoot: entry");
	status_led->set(RGBLedColor::WHITE);
}

void LEDPowerDown::entry() {
	DEBUG_TRACE("LEDPowerDown: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::WHITE, 125);
}

void LEDError::entry() {
	DEBUG_TRACE("LEDError: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::RED);
}

void LEDPreOperationalError::entry() {
	DEBUG_TRACE("LEDPreOperationalError: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::RED);
}

void LEDPreOperationalBatteryNominal::entry() {
	DEBUG_TRACE("LEDPreOperationalBatteryNominal: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::GREEN);
}

void LEDPreOperationalBatteryLow::entry() {
	DEBUG_TRACE("LEDPreOperationalBatteryLow: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::YELLOW);
}

void LEDConfigNotConnected::entry() {
	DEBUG_TRACE("LEDConfigNotConnected: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::BLUE);
}

void LEDConfigConnected::entry() {
	DEBUG_TRACE("LEDConfigConnected: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->set(RGBLedColor::BLUE);
}

void LEDGNSSOn::entry() {
	DEBUG_TRACE("LEDGNSSOn: entry");
	m_is_gnss_on = true;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::CYAN, 1000);
}

void LEDGNSSOffWithFix::entry() {
	DEBUG_TRACE("LEDGNSSOffWithFix: entry");
	m_is_gnss_on = false;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else {
		status_led->set(RGBLedColor::GREEN);
		system_timer->add_schedule([this]() {
			transit<LEDOff>();
		}, system_timer->get_counter() + 3000);
	}
}

void LEDGNSSOffWithoutFix::entry() {
	DEBUG_TRACE("LEDGNSSOffWithoutFix: entry");
	m_is_gnss_on = false;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else {
		status_led->set(RGBLedColor::RED);
		system_timer->add_schedule([this]() {
			transit<LEDOff>();
		}, system_timer->get_counter() + 3000);
	}
}

void LEDArgosTX::entry() {
	DEBUG_TRACE("LEDArgosTX: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->set(RGBLedColor::MAGENTA);
}

void LEDArgosTXComplete::entry() {
	DEBUG_TRACE("LEDArgosTXComplete: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		system_timer->add_schedule([this]() {
			if (m_is_gnss_on)
				transit<LEDGNSSOn>();
			else
				transit<LEDOff>();
		}, system_timer->get_counter());
}
