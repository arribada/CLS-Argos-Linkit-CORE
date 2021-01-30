#ifndef __GENTRACKER_HPP_INC
#define __GENTRACKER_HPP_INC

#include "tinyfsm.hpp"
#include "scheduler.hpp"
#include "error.hpp"

struct ReedSwitchEvent              : tinyfsm::Event { bool state; };
struct SaltwaterSwitchEvent         : tinyfsm::Event { bool state; };
struct ErrorEvent : tinyfsm::Event { ErrorCode error_code; };


class GenTracker : public tinyfsm::Fsm<GenTracker>
{
private:
	static inline Scheduler::TaskHandle m_task_trigger_config_state;
	static inline Scheduler::TaskHandle m_task_trigger_off_state;
	static inline const unsigned int TRANSIT_CONFIG_HOLD_TIME_MS = 3000;
	static inline const unsigned int TRANSIT_OFF_HOLD_TIME_MS = 10000;
	static inline const unsigned int SWIPE_TIME_MS = 500;
	static inline uint64_t m_reed_trigger_start_time;

public:
	void react(tinyfsm::Event const &);
	void react(ReedSwitchEvent const &event);
	void react(SaltwaterSwitchEvent const &);
	void react(ErrorEvent const &event);
	virtual void entry(void);
	virtual void exit(void);

	static void notify_bad_filesystem_error();
};


class BootState : public GenTracker
{
public:
	void entry() override;
};

class OffState : public GenTracker
{
private:
	static inline const unsigned int OFF_LED_PERIOD_MS = 6000;

public:
	void entry() override;
	void exit() override;
};

class IdleState : public GenTracker
{
private:
	static inline const unsigned int IDLE_PERIOD_MS = 120 * 1000;
	Scheduler::TaskHandle m_idle_state_task;

public:
	void entry() override;
	void exit() override;
};

class OperationalState : public GenTracker
{
private:
	static inline const unsigned int LED_INDICATION_PERIOD_MS = 5000;

public:
	void react(SaltwaterSwitchEvent const &event);
	void entry() override;
	void exit() override;
};

class ConfigurationState : public GenTracker
{
private:
	static inline const unsigned int BLE_INACTIVITY_TIMEOUT_MS = 6 * 60 * 1000;
	Scheduler::TaskHandle m_ble_inactivity_timeout_task;

	void on_dte_connected();
	void on_dte_disconnected();
	void on_dte_inactivity_timeout();
	void on_dte_received();
	void restart_inactivity_timeout();
	void process_received_data();

public:
	void entry() override;
	void exit() override;
};

class ErrorState : public GenTracker
{
public:
	void entry();
	void exit();
};

#endif // __GENTRACKER_HPP_INC
