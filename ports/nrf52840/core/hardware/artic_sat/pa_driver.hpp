#pragma once
#include <cstdint>

#include "calibration.hpp"

class PAInterface {
public:
	virtual ~PAInterface() {}
	virtual void set_output_power(unsigned int mW) = 0;
};

class PADriver : public PAInterface {
public:
	PADriver();
	~PADriver();
	void set_output_power(unsigned int mW) override;
	static void shutdown();

private:
	PAInterface *m_interface;
};


class DummyPA : public PAInterface {
public:
	DummyPA() {}
	void set_output_power(unsigned int) override {
	}
	static void shutdown() {}
};


class RFPA133 : public PAInterface {
public:
	RFPA133();
	void set_output_power(unsigned int mW) override;
	static void shutdown();
};

class MCP47X6 : public PAInterface, public Calibratable {
private:
	// Programmable Gain definitions
	static inline const uint8_t MCP47X6_GAIN_MASK = 0xFE;
	static inline const uint8_t MCP47X6_GAIN_1X	  = 0x00;
	static inline const uint8_t MCP47X6_GAIN_2X	  = 0x01;

	// Power Down Mode definitions
	static inline const uint8_t MCP47X6_PWRDN_MASK = 0xF9;
	static inline const uint8_t MCP47X6_AWAKE      = 0x00;
	static inline const uint8_t MCP47X6_PWRDN_1K   = 0x02;
	static inline const uint8_t MCP47X6_PWRDN_100K = 0x04;
	static inline const uint8_t MCP47X6_PWRDN_500K = 0x06;

	// Reference Voltage definitions
	static inline const uint8_t MCP47X6_VREF_MASK             = 0xE7;
	static inline const uint8_t MCP47X6_VREF_VDD              = 0x00;
	static inline const uint8_t MCP47X6_VREF_VREFPIN	      = 0x10;
	static inline const uint8_t MCP47X6_VREF_VREFPIN_BUFFERED = 0x18;

	// Command definitions
	static inline const uint8_t MCP47X6_CMD_MASK       = 0x1F;
	static inline const uint8_t MCP47X6_CMD_VOLDAC     = 0x00;
	static inline const uint8_t MCP47X6_CMD_VOLALL     = 0x40;
	static inline const uint8_t MCP47X6_CMD_VOLCONFIG  = 0x80;
	static inline const uint8_t MCP47X6_CMD_ALL        = 0x60;

	Calibration m_cal;
	uint8_t m_config_reg;

	void set_vref(uint8_t vref);
	void set_gain(uint8_t gain);
	void set_level(uint16_t level);
	void power_down(void);

public:
	MCP47X6();
	~MCP47X6();
	void set_output_power(unsigned int mW) override;
	void calibration_write(const double value, const unsigned int offset) override;
	static void shutdown();
};
