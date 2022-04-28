#pragma once

#include "tinyfsm.hpp"
#include "scheduler.hpp"
#include "error.hpp"
#include "ble_service.hpp"
#include "reed.hpp"

struct ReedSwitchEvent              : tinyfsm::Event { ReedSwitchGesture state; };
struct SaltwaterSwitchEvent         : tinyfsm::Event { bool state; };
struct ErrorEvent : tinyfsm::Event { ErrorCode error_code; };


class GenTracker : public tinyfsm::Fsm<GenTracker>
{
public:
	void react(tinyfsm::Event const &);
	void react(ReedSwitchEvent const &event);
	virtual void react(SaltwaterSwitchEvent const &event);
	void react(ErrorEvent const &event);
	virtual void entry(void);
	virtual void exit(void);
	void set_ble_device_name();

	static void kick_watchdog();
	static void notify_bad_filesystem_error();
};


class BootState : public GenTracker
{
public:
	void entry() override;
	void exit() override;
};

class OffState : public GenTracker
{
private:
	static inline const unsigned int OFF_LED_PERIOD_MS = 5000;
	Scheduler::TaskHandle m_off_state_task;

public:
	void entry() override;
	void exit() override;
};

class PreOperationalState : public GenTracker
{
private:
	static inline const unsigned int TRANSIT_PERIOD_MS = 5000;
	Scheduler::TaskHandle m_preop_state_task;

public:
	void entry() override;
	void exit() override;
};

class OperationalState : public GenTracker
{
public:
	void react(SaltwaterSwitchEvent const &event) override;
	void entry() override;
	void exit() override;
};

class ConfigurationState : public GenTracker
{
private:
	static inline const unsigned int BLE_INACTIVITY_TIMEOUT_MS = 6 * 60 * 1000;
	Scheduler::TaskHandle m_ble_inactivity_timeout_task;
	int on_ble_event(BLEServiceEvent&);
	void on_ble_inactivity_timeout();
	void restart_inactivity_timeout();
	void process_received_data();

public:
	void entry() override;
	void exit() override;
};

class ErrorState : public GenTracker
{
private:
	Scheduler::TaskHandle m_shutdown_task;
public:
	void entry();
	void exit();
};
