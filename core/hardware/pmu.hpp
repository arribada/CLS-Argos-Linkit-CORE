#pragma once

class PMU {
public:
	static void initialise();
	static void reset();
	static void powerdown();
	static void run();
};
