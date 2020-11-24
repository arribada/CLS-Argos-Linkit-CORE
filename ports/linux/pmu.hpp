#ifndef __PMU_HPP_
#define __PMU_HPP_

extern "C" {
#include <unistd.h>
}

class PMU {
public:
	static void enter_low_power_mode() {
		usleep(100);
	}
};

#endif // __PMU_HPP_
