#pragma once

#include "tinyfsm.hpp"
#include "timer.hpp"
#include "rgb_led.hpp"


struct SetLEDOff : tinyfsm::Event { };
struct SetLEDMagnetEngaged : tinyfsm::Event { };
struct SetLEDMagnetDisengaged : tinyfsm::Event { };
struct SetLEDBoot : tinyfsm::Event { };
struct SetLEDPowerDown : tinyfsm::Event { };
struct SetLEDError : tinyfsm::Event { };
struct SetLEDPreOperationalPending : tinyfsm::Event { };
struct SetLEDPreOperationalError : tinyfsm::Event { };
struct SetLEDPreOperationalBatteryNominal : tinyfsm::Event { };
struct SetLEDPreOperationalBatteryLow : tinyfsm::Event { };
struct SetLEDConfigPending : tinyfsm::Event { };
struct SetLEDConfigNotConnected : tinyfsm::Event { };
struct SetLEDConfigConnected : tinyfsm::Event { };
struct SetLEDGNSSOn : tinyfsm::Event { };
struct SetLEDGNSSOffWithFix : tinyfsm::Event { };
struct SetLEDGNSSOffWithoutFix : tinyfsm::Event { };
struct SetLEDArgosTX : tinyfsm::Event { };
struct SetLEDArgosTXComplete : tinyfsm::Event { };

class LEDOff;
class LEDBoot;
class LEDPowerDown;
class LEDError;
class LEDPreOperationalPending;
class LEDPreOperationalBatteryNominal;
class LEDPreOperationalBatteryLow;
class LEDPreOperationalError;
class LEDConfigPending;
class LEDConfigNotConnected;
class LEDConfigConnected;
class LEDGNSSOn;
class LEDGNSSOffWithFix;
class LEDGNSSOffWithoutFix;
class LEDArgosTX;
class LEDArgosTXComplete;


class LEDState : public tinyfsm::Fsm<LEDState> {
protected:
	static inline bool m_is_gnss_on = false;
	static inline bool m_is_magnet_engaged = false;
public:
	void react(SetLEDOff const &) { transit<LEDOff>(); }
	void react(SetLEDMagnetEngaged const &) { if (!m_is_magnet_engaged) { m_is_magnet_engaged = true; enter(); } }
	void react(SetLEDMagnetDisengaged const &) { if (m_is_magnet_engaged) { m_is_magnet_engaged = false; enter(); } }
	void react(SetLEDBoot const &) { transit<LEDBoot>(); }
	void react(SetLEDPowerDown const &) { transit<LEDPowerDown>(); }
	void react(SetLEDError const &) { transit<LEDError>(); }
	void react(SetLEDPreOperationalPending const &) { transit<LEDPreOperationalPending>(); }
	void react(SetLEDPreOperationalError const &) { transit<LEDPreOperationalError>(); }
	void react(SetLEDPreOperationalBatteryNominal const &) { transit<LEDPreOperationalBatteryNominal>(); }
	void react(SetLEDPreOperationalBatteryLow const &) { transit<LEDPreOperationalBatteryLow>(); }
	void react(SetLEDConfigPending const &) { transit<LEDConfigPending>(); }
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

class LEDError : public LEDState
{
public:
	void entry() override;
	void exit() override {};
};

class LEDPreOperationalPending : public LEDState
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

class LEDConfigPending : public LEDState
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
