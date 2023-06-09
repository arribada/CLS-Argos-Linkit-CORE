#include "ledsm.hpp"
#include "debug.hpp"
#include "config_store.hpp"
#include "rgb_led.hpp"
#include "led.hpp"

extern Led *ext_status_led;
extern RGBLed *status_led;
extern Timer *system_timer;
extern ConfigurationStore *configuration_store;


// Macro for determining LED mode state
#define LED_MODE_GUARD \
	if (configuration_store->read_param<BaseLEDMode>(ParamID::LED_MODE) == BaseLEDMode::ALWAYS || \
		(configuration_store->read_param<BaseLEDMode>(ParamID::LED_MODE) == BaseLEDMode::HRS_24 && \
		 system_timer->get_counter() < (24 * 3600 * 1000)))

#define EXT_LED_MODE_GUARD \
	if (configuration_store->read_param<BaseLEDMode>(ParamID::EXT_LED_MODE) == BaseLEDMode::ALWAYS || \
		(configuration_store->read_param<BaseLEDMode>(ParamID::EXT_LED_MODE) == BaseLEDMode::HRS_24 && \
		 system_timer->get_counter() < (24 * 3600 * 1000)))


void LEDOff::entry() {
	DEBUG_TRACE("LEDOff: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else if (m_is_battery_critical)
		transit<LEDBatteryCritical>();
	else
		status_led->off();
	if (ext_status_led)
		ext_status_led->off();
}

void LEDBoot::entry() {
	DEBUG_TRACE("LEDBoot: entry");
	status_led->flash(RGBLedColor::WHITE, 125);
	if (ext_status_led)
		ext_status_led->flash(125);
}

void LEDPowerDown::entry() {
	DEBUG_TRACE("LEDPowerDown: entry");
	m_is_battery_critical = false;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::WHITE, 50);
	if (ext_status_led)
		ext_status_led->flash(50);
}

void LEDError::entry() {
	DEBUG_TRACE("LEDError: entry");
	m_is_battery_critical = false;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::RED);
	if (ext_status_led)
		ext_status_led->off();
}

void LEDPreOperationalPending::entry() {
	DEBUG_TRACE("LEDPreOperationalPending: entry");
	m_is_battery_critical = false;
	status_led->set(RGBLedColor::GREEN);
	if (ext_status_led)
		ext_status_led->off();
}

void LEDPreOperationalError::entry() {
	DEBUG_TRACE("LEDPreOperationalError: entry");
	m_is_battery_critical = false;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::RED);
	if (ext_status_led)
		ext_status_led->off();
}

void LEDPreOperationalBatteryNominal::entry() {
	DEBUG_TRACE("LEDPreOperationalBatteryNominal: entry");
	m_is_battery_critical = false;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::GREEN);
	if (ext_status_led)
		ext_status_led->off();
}

void LEDPreOperationalBatteryLow::entry() {
	DEBUG_TRACE("LEDPreOperationalBatteryLow: entry");
	m_is_battery_critical = false;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::YELLOW);
	if (ext_status_led)
		ext_status_led->off();
}

void LEDConfigPending::entry() {
	DEBUG_TRACE("LEDConfigPending: entry");
	m_is_battery_critical = false;
	status_led->set(RGBLedColor::BLUE);
	if (ext_status_led)
		ext_status_led->off();
}

void LEDConfigNotConnected::entry() {
	DEBUG_TRACE("LEDConfigNotConnected: entry");
	m_is_battery_critical = false;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->flash(RGBLedColor::BLUE);
	if (ext_status_led)
		ext_status_led->off();
}

void LEDConfigConnected::entry() {
	DEBUG_TRACE("LEDConfigConnected: entry");
	m_is_battery_critical = false;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else
		status_led->set(RGBLedColor::BLUE);
	if (ext_status_led)
		ext_status_led->off();
}

void LEDGNSSOn::entry() {
	DEBUG_TRACE("LEDGNSSOn: entry");
	m_is_gnss_on = true;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else {
		LED_MODE_GUARD {
			status_led->flash(RGBLedColor::CYAN, 1000);
		} else {
			status_led->off();
		}
		EXT_LED_MODE_GUARD {
			if (ext_status_led)
				ext_status_led->on();
		} else {
			if (ext_status_led)
				ext_status_led->off();
		}
	}
}

void LEDGNSSOffWithFix::entry() {
	DEBUG_TRACE("LEDGNSSOffWithFix: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else {
		if (m_is_gnss_on) {
			LED_MODE_GUARD {
				status_led->set(RGBLedColor::GREEN);
			} else {
				status_led->off();
			}
			if (ext_status_led)
				ext_status_led->off();
		}
		system_timer->add_schedule([this]() {
			if (is_in_state<LEDConfigNotConnected>())
				transit<LEDConfigNotConnected>();
			else
				transit<LEDOff>();
		}, system_timer->get_counter() + 3000);
	}
	m_is_gnss_on = false;
}

void LEDGNSSOffWithoutFix::entry() {
	DEBUG_TRACE("LEDGNSSOffWithoutFix: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else {
		if (m_is_gnss_on) {
			LED_MODE_GUARD {
				status_led->set(RGBLedColor::RED);
			} else {
				status_led->off();
			}
			if (ext_status_led)
				ext_status_led->off();
		}
		system_timer->add_schedule([this]() {
			if (is_in_state<LEDConfigNotConnected>())
				transit<LEDConfigNotConnected>();
			else
				transit<LEDOff>();
		}, system_timer->get_counter() + 3000);
	}
	m_is_gnss_on = false;
}

void LEDArgosTX::entry() {
	DEBUG_TRACE("LEDArgosTX: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else {
		LED_MODE_GUARD {
			status_led->set(RGBLedColor::MAGENTA);
		} else {
			status_led->off();
		}
	}
}

void LEDArgosTXComplete::entry() {
	DEBUG_TRACE("LEDArgosTXComplete: entry");
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else {
		status_led->off();
		system_timer->add_schedule([this]() {
			if (m_is_gnss_on)
				transit<LEDGNSSOn>();
			else {
				if (is_in_state<LEDConfigNotConnected>())
					transit<LEDConfigNotConnected>();
				else
					transit<LEDOff>();
			}
		}, system_timer->get_counter() + 50); // Add 50ms to avoid race condition on timer tick
	}
}

void LEDBatteryCritical::entry() {
	DEBUG_TRACE("LEDBatteryCritical: entry");
	m_is_battery_critical = true;
	if (m_is_magnet_engaged)
		status_led->set(RGBLedColor::WHITE);
	else {
		status_led->set(RGBLedColor::YELLOW);
	}
}
