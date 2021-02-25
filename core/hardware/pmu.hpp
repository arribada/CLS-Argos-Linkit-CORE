#pragma once

class PMU {
public:
	static void initialise();
	static void reset(bool dfu_mode);
	static void powerdown();
	static void run();
};
