#pragma once

#include <cstdint>

enum class VRange {
	V1_GAIN1X,
	V2_GAIN1X,
	V200MV_GAIN1X,
	V400MV_GAIN1X,
	V1_GAIN0_5X,
	V2_GAIN0_5X,
	V200MV_GAIN0_5X,
	V400MV_GAIN0_5X,
};

class AD5933 {
public:
	virtual ~AD5933() {}
	virtual void start(const unsigned int frequency, const VRange vrange) = 0;
	virtual void stop() = 0;
	virtual double get_impedence(const unsigned int averaging, const double gain) = 0;
	virtual void get_real_imaginary(int16_t& real, int16_t& imag) = 0;
};

class AD5933LL : public AD5933 {
public:
	AD5933LL(unsigned int bus, unsigned char addr);
	void start(const unsigned int frequency, const VRange vrange);
	void stop();
	double get_impedence(const unsigned int averaging, const double gain);
	void get_real_imaginary(int16_t& real, int16_t& imag);

private:
	unsigned int m_bus;
	unsigned char m_addr;
	uint8_t m_gain_setting;

	enum class AD5933Register : uint8_t {
		CONTROL_HIGH = 0x80,
		CONTROL_LOW = 0x81,
		START_FREQUENCY_23_16 = 0x82,
		START_FREQUENCY_15_8 = 0x83,
		START_FREQUENCY_7_0 = 0x84,
		FREQUENCY_INCREMENT_23_16 = 0x85,
		FREQUENCY_INCREMENT_15_8 = 0x86,
		FREQUENCY_INCREMENT_7_0 = 0x87,
		NUM_INCREMENTS_15_8 = 0x88,
		NUM_INCREMENTS_7_0 = 0x89,
		SETTLING_TIMES_15_8 = 0x8A,
		SETTLING_TIMES_7_0 = 0x8B,
		STATUS = 0x8F,
		TEMPERATURE_15_8 = 0x92,
		TEMPERATURE_7_0 = 0x93,
		REAL_15_8 = 0x94,
		REAL_7_0 = 0x95,
		IMAG_15_8 = 0x96,
		IMAG_7_0 = 0x97,
	};
	enum class AD5933ControlRegisterHigh : uint8_t {
		INIT_START_FREQUENCY = (1 << 4),
		START_FREQUENCY_SWEEP = (2 << 4),
		INCREMENT_FREQUENCY = (3 << 4),
		REPEAT_FREQUENCY = (4 << 4),
		MEASURE_TEMPERATURE = (9 << 4),
		POWER_DOWN = (10 << 4),
		STANDBY = (11 << 4),
		OPV_RANGE_2V_PP = (0 << 1),
		OPV_RANGE_200MV_PP = (1 << 1),
		OPV_RANGE_400MV_PP = (2 << 1),
		OPV_RANGE_1V_PP = (3 << 1),
		PGA_GAIN_1X = (1 << 0),
	};
	enum class AD5933ControlRegisterLow : uint8_t {
		RESET = (1 << 4),
		EXT_SYS_CLOCK = (1 << 3)
	};
	enum class AD5933StatusRegister : uint8_t {
		VALID_TEMPERATURE = (1 << 0),
		VALID_REAL_IMAG_DATA = (1 << 1),
		FREQ_SWEEP_COMPLETE = (1 << 2),
	};

	uint8_t read_reg(const AD5933Register reg);
	void write_reg(const AD5933Register reg, uint8_t value);
	void set_start_frequency(unsigned int frequency);
	void set_number_of_increments(unsigned int num);
	void set_frequency_increment(unsigned int inc);
	void set_settling_times(unsigned int settling);
	void initialize();
	void reset();
	void standby();
	void startsweep();
	void wait_iq_data_ready();
	void powerdown();
	void gain(VRange);
	uint8_t status();
	int16_t read_real();
	int16_t read_imag();
	void dump_regs();
};
