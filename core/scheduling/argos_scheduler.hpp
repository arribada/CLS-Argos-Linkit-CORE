#ifndef __ARGOS_SCHEDULER_HPP_
#define __ARGOS_SCHEDULER_HPP_

class ArgosScheduler {
public:
	static void start();
	static void stop();
	static void notify_saltwater_switch_state(bool);
};

#endif // __ARGOS_SCHEDULER_HPP_
