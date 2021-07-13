#pragma once

#include "tinyfsm.hpp"
#include "timer.hpp"
#include "rgb_led.hpp"


enum class LEDMenuState {
	INACTIVE,
	CONFIG,
	OPERATIONAL,
	POWERDOWN
};

struct SetLEDOff : tinyfsm::Event { };
struct SetLEDMenu : tinyfsm::Event { };
struct SetLEDBoot : tinyfsm::Event { };
struct SetLEDPowerDown : tinyfsm::Event { };
struct SetLEDPreOperationalError : tinyfsm::Event { };
struct SetLEDPreOperationalBatteryNominal : tinyfsm::Event { };
struct SetLEDPreOperationalBatteryLow : tinyfsm::Event { };
struct SetLEDError : tinyfsm::Event { };
struct SetLEDOperationalBatteryNominal : tinyfsm::Event { };
struct SetLEDOperationalBatteryLow : tinyfsm::Event { };
struct SetLEDConfigNotConnected : tinyfsm::Event { };
struct SetLEDConfigConnected : tinyfsm::Event { };
struct SetLEDGNSSOn : tinyfsm::Event { };
struct SetLEDGNSSOffWithFix : tinyfsm::Event { };
struct SetLEDGNSSOffWithoutFix : tinyfsm::Event { };
struct SetLEDArgosTX : tinyfsm::Event { };
struct SetLEDArgosTXComplete : tinyfsm::Event { };

class LEDOff;
class LEDMenu;
class LEDBoot;
class LEDPowerDown;
class LEDPreOperationalError;
class LEDPreOperationalBatteryNominal;
class LEDPreOperationalBatteryLow;
class LEDError;
class LEDOperationalBatteryNominal;
class LEDOperationalBatteryLow;
class LEDConfigNotConnected;
class LEDConfigConnected;
class LEDGNSSOn;
class LEDGNSSOffWithFix;
class LEDGNSSOffWithoutFix;
class LEDArgosTX;
class LEDArgosTXComplete;


class LEDState : public tinyfsm::Fsm<LEDState> {
protected:
	static inline LEDMenuState m_last_menu_state = LEDMenuState::INACTIVE;
	static inline bool m_is_gnss_on = false;
	static inline Timer::TimerHandle m_timer_handle;
public:
	LEDMenuState get_last_menu_state() { return m_last_menu_state; }
	static LEDMenuState next_menu_state(const LEDMenuState s) {
		int x = (int)s + 1;
		if (x > (int)LEDMenuState::POWERDOWN)
			x = (int)LEDMenuState::CONFIG;
		return (LEDMenuState)x;
	}
	static RGBLedColor menu_color(const LEDMenuState s) {
		if (LEDMenuState::CONFIG == s)
			return RGBLedColor::BLUE;
		else if (LEDMenuState::OPERATIONAL == s)
			return RGBLedColor::GREEN;
		else if (LEDMenuState::POWERDOWN == s)
			return RGBLedColor::WHITE;
		return RGBLedColor::BLACK;
	}

	void react(SetLEDOff const &) { transit<LEDOff>(); }
	void react(SetLEDMenu const &) { transit<LEDMenu>(); }
	void react(SetLEDBoot const &) { transit<LEDBoot>(); }
	void react(SetLEDPowerDown const &) { transit<LEDPowerDown>(); }
	void react(SetLEDPreOperationalError const &) { transit<LEDPreOperationalError>(); }
	void react(SetLEDPreOperationalBatteryNominal const &) { transit<LEDPreOperationalBatteryNominal>(); }
	void react(SetLEDPreOperationalBatteryLow const &) { transit<LEDPreOperationalBatteryLow>(); }
	void react(SetLEDError const &) { transit<LEDError>(); }
	void react(SetLEDOperationalBatteryNominal const &) { transit<LEDOperationalBatteryNominal>(); }
	void react(SetLEDOperationalBatteryLow const &) { transit<LEDOperationalBatteryLow>(); }
	void react(SetLEDConfigNotConnected const &) { transit<LEDConfigNotConnected>(); }
	void react(SetLEDConfigConnected const &) { transit<LEDConfigConnected>(); }
	void react(SetLEDGNSSOn const &) { transit<LEDGNSSOn>(); }
	void react(SetLEDGNSSOffWithFix const &) { transit<LEDGNSSOffWithFix>(); }
	void react(SetLEDGNSSOffWithoutFix const &) { transit<LEDGNSSOffWithoutFix>(); }
	void react(SetLEDArgosTX const &) { transit<LEDArgosTX>(); }
	void react(SetLEDArgosTXComplete const &) { transit<LEDArgosTXComplete>(); }

	virtual void entry(void) {}
	virtual void exit(void) {}
};


class LEDOff : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};


class LEDMenu : public LEDState
{
public:
	void entry() override;
	void exit(void) override;
};

class LEDBoot : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDPowerDown : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDPreOperationalError : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDPreOperationalBatteryNominal : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDPreOperationalBatteryLow : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDError : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDOperationalBatteryNominal : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDOperationalBatteryLow : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDConfigNotConnected : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDConfigConnected : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDGNSSOn : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDGNSSOffWithFix : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDGNSSOffWithoutFix : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDArgosTX : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDArgosTXComplete : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};
