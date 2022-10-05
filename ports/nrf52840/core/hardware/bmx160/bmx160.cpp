#include <map>

#include "bmx160.h"
#include "bmx160.hpp"

#include "debug.hpp"
#include "nrf_delay.h"
#include "bsp.hpp"
#include "nrfx_twim.h"
#include "pmu.hpp"
#include "error.hpp"
#include "nrf_irq.hpp"


class BMX160LLManager {
private:
	static inline uint8_t m_unique_id = 0;
	static inline std::map<uint8_t, BMX160LL&> m_map;

public:
	static uint8_t register_device(BMX160LL& dev);
	static void unregister_device(uint8_t unique_id);
	static BMX160LL& lookup_device(uint8_t unique_id);
};

BMX160LL& BMX160LLManager::lookup_device(uint8_t unique_id) {
	return m_map.at(unique_id);
}

uint8_t BMX160LLManager::register_device(BMX160LL& device) {
	m_map.insert({m_unique_id, device});
	return m_unique_id++;
}

void BMX160LLManager::unregister_device(uint8_t unique_id) {
	m_map.erase(unique_id);
}

BMX160LL::BMX160LL(unsigned int bus, unsigned char addr, int wakeup_pin) : m_bus(bus), m_addr(addr), m_irq(NrfIRQ(wakeup_pin)),
		m_unique_id(BMX160LLManager::register_device(*this)), m_accel_sleep_mode(BMX160_ACCEL_SUSPEND_MODE),
		m_irq_pending(false) {
	try {
		init();
	} catch(...) {
		BMX160LLManager::unregister_device(m_unique_id);
		throw;
	}
}

BMX160LL::~BMX160LL() {
	BMX160LLManager::unregister_device(m_unique_id);
}

void BMX160LL::init()
{
	DEBUG_TRACE("BMX160LL::init");
    m_bmx160_dev.id = m_unique_id;
    m_bmx160_dev.interface = BMX160_I2C_INTF;

    m_bmx160_dev.write = i2c_write;
    m_bmx160_dev.read = i2c_read;
    m_bmx160_dev.delay_ms = nrf_delay_ms;

    int8_t err_code;

    err_code = bmx160_init(&m_bmx160_dev);

    if (err_code != BMX160_OK)
    {
        DEBUG_ERROR("BMX160 initialization failure");
        throw ErrorCode::I2C_COMMS_ERROR;
    }

    m_bmx160_dev.accel_cfg.bw = BMX160_ACCEL_BW_NORMAL_AVG4;
    m_bmx160_dev.accel_cfg.odr = BMX160_ACCEL_ODR_100HZ;
    m_bmx160_dev.accel_cfg.power = BMX160_ACCEL_SUSPEND_MODE;
    m_bmx160_dev.accel_cfg.range = BMX160_ACCEL_RANGE_16G;

    m_bmx160_dev.gyro_cfg.bw = BMX160_GYRO_BW_NORMAL_MODE;
    m_bmx160_dev.gyro_cfg.odr = BMX160_GYRO_ODR_100HZ;
    m_bmx160_dev.gyro_cfg.power = BMX160_GYRO_SUSPEND_MODE;
    m_bmx160_dev.gyro_cfg.range = BMX160_GYRO_RANGE_2000_DPS;

    err_code = bmx160_set_sens_conf(&m_bmx160_dev);
    if (err_code != BMX160_OK)
    {
        DEBUG_ERROR("BMX160 initialization failure");
        throw ErrorCode::I2C_COMMS_ERROR;
    }
}

int8_t BMX160LL::i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    nrfx_err_t err_code;
    BMX160LL& device = BMX160LLManager::lookup_device(dev_addr);

    if (!len)
        return BMX160_OK;
    
    if (len > BMX160_MAX_BUFFER_LEN + sizeof(reg_addr))
        return BMX160_E_OUT_OF_RANGE;
    
    uint8_t buffer[BMX160_MAX_BUFFER_LEN];
    buffer[0] = reg_addr;
    memcpy(&buffer[1], data, len);

    err_code = nrfx_twim_tx(&BSP::I2C_Inits[device.m_bus].twim, device.m_addr, buffer, len + sizeof(reg_addr), false);
    if (err_code != NRFX_SUCCESS)
        return BMX160_E_COM_FAIL;

    return BMX160_OK;
}

int8_t BMX160LL::i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    nrfx_err_t err_code;
    BMX160LL& device = BMX160LLManager::lookup_device(dev_addr);

    err_code = nrfx_twim_tx(&BSP::I2C_Inits[device.m_bus].twim, device.m_addr, &reg_addr, sizeof(reg_addr), true);
    if (err_code != NRFX_SUCCESS)
        return BMX160_E_COM_FAIL;
    
    err_code = nrfx_twim_rx(&BSP::I2C_Inits[device.m_bus].twim, device.m_addr, data, len);
    if (err_code != NRFX_SUCCESS)
        return BMX160_E_COM_FAIL;

    return BMX160_OK;
}

double BMX160LL::convert_g_force(unsigned int g_scale, int16_t axis_value) {
	return (double)g_scale * axis_value / 32768;
}

void BMX160LL::read_xyz(double& x, double& y, double& z) {
    // Turn accelerometer on so AXL is updated
    m_bmx160_dev.accel_cfg.power = BMX160_ACCEL_NORMAL_MODE;
    bmx160_set_power_mode(&m_bmx160_dev);

    // Wait 50ms for reading (4 averaged samples, @ 100 Hz)
    PMU::delay_ms(50);

    // Read and convert accelerometer values
    union __attribute__((packed)) {
        uint8_t buffer[6];
        struct {
        	int16_t x;
        	int16_t y;
        	int16_t z;
        };
    } data;
    bmx160_get_regs(BMX160_ACCEL_DATA_ADDR, data.buffer, sizeof(data.buffer), &m_bmx160_dev);

    // Convert to double precision G-force result on each axis
    x = convert_g_force(16, data.x);
    y = convert_g_force(16, data.y);
    z = convert_g_force(16, data.z);

    DEBUG_TRACE("BMX160LL::read_xyz: xyz=%f,%f,%f", x, y, z);

    // Turn accelerometer on so AXL is updated
    m_bmx160_dev.accel_cfg.power = m_accel_sleep_mode;
    bmx160_set_power_mode(&m_bmx160_dev);
}

double BMX160LL::read_temperature()
{
    // Turn gyro on so temperature is updated
    m_bmx160_dev.gyro_cfg.power = BMX160_GYRO_NORMAL_MODE;
    bmx160_set_power_mode(&m_bmx160_dev);

    // Wait 10ms +/-12% for temperature reading
    PMU::delay_us(11200);

    // Read temperature
    uint8_t buffer[2];
    bmx160_get_regs(BMX160_TEMPERATURE_ADDR, buffer, sizeof(buffer), &m_bmx160_dev);

    // Suspend gyro
    m_bmx160_dev.gyro_cfg.power = BMX160_GYRO_SUSPEND_MODE;
    bmx160_set_power_mode(&m_bmx160_dev);

    // Convert and return value
    int16_t raw;
    raw = (int16_t)((static_cast<uint16_t>(buffer[1]) << 8) | buffer[0]);
    return (double)raw * 0.001953125 + 23;
}

void BMX160LL::set_wakeup_threshold(double thresh) {
	m_wakeup_threshold = thresh;
}

void BMX160LL::set_wakeup_duration(double duration) {
	m_wakeup_duration = duration;
}

void BMX160LL::enable_wakeup(std::function<void()> func) {
	// Enable IRQ
	m_accel_sleep_mode = m_bmx160_dev.accel_cfg.power = BMX160_ACCEL_LOWPOWER_MODE;
	bmx160_set_power_mode(&m_bmx160_dev);
	bmx160_int_pin_settg int_pin_settg =
	{
		.output_en = 1,
		.output_mode = 0,
		.output_type = 1,
		.edge_ctrl = 0,
		.input_en = 0,
		.latch_dur = 1
	};
	uint8_t anymotion_thr = (uint8_t)(std::max(255.0, (m_wakeup_threshold * 1000) / 31.25));
	bmx160_acc_any_mot_int_cfg acc_any_motion_int = {
		.anymotion_en = 1,
		.anymotion_x = 1,
		.anymotion_y = 1,
		.anymotion_z = 1,
		.anymotion_dur = (uint8_t)(m_wakeup_duration - 1),
		.anymotion_data_src = 1,
		.anymotion_thr = anymotion_thr
	};
	bmx160_int_type_cfg int_type_cfg;
	int_type_cfg.acc_any_motion_int = acc_any_motion_int;
	bmx160_int_settg int_setting = {
		.int_channel = BMX160_INT_CHANNEL_1,
		.int_type = BMX160_ACC_ANY_MOTION_INT,
		.int_pin_settg = int_pin_settg,
		.int_type_cfg = int_type_cfg,
		.fifo_full_int_en = 0,
		.fifo_WTM_int_en = 0,
	};
	bmx160_set_int_config(&int_setting, &m_bmx160_dev);
	m_irq.enable([this, func]() {
		if (!m_irq_pending) {
			m_irq_pending = true;
			func();
		}
	});
}

void BMX160LL::disable_wakeup() {
	// Disable IRQ
	m_irq.disable();
	m_accel_sleep_mode = m_bmx160_dev.accel_cfg.power = BMX160_ACCEL_SUSPEND_MODE;
	bmx160_set_power_mode(&m_bmx160_dev);
	bmx160_int_pin_settg int_pin_settg =
	{
		.output_en = 0,
		.output_mode = 0,
		.output_type = 1,
		.edge_ctrl = 0,
		.input_en = 0,
		.latch_dur = 1
	};
	bmx160_acc_any_mot_int_cfg acc_any_motion_int = {
		.anymotion_en = 0,
		.anymotion_x = 0,
		.anymotion_y = 0,
		.anymotion_z = 0,
		.anymotion_dur = 1,
		.anymotion_data_src = 0,
		.anymotion_thr = 0
	};
	bmx160_int_type_cfg int_type_cfg;
	int_type_cfg.acc_any_motion_int = acc_any_motion_int;
	bmx160_int_settg int_setting = {
		.int_channel = BMX160_INT_CHANNEL_1,
		.int_type = BMX160_ACC_ANY_MOTION_INT,
		.int_pin_settg = int_pin_settg,
		.int_type_cfg = int_type_cfg,
		.fifo_full_int_en = 0,
		.fifo_WTM_int_en = 0,
	};
	bmx160_set_int_config(&int_setting, &m_bmx160_dev);
	m_irq.disable();
}

bool BMX160LL::check_and_clear_wakeup() {
	InterruptLock lock;
	bool value = m_irq_pending;
	m_irq_pending = false;
	DEBUG_TRACE("BMX160LL::check_and_clear_wakeup: pending=%u", (unsigned int)value);
	return value;
}

BMX160::BMX160() : Sensor("AXL"), m_bmx160(BMX160LL(BMX160_DEVICE, BMX160_ADDRESS, BMX160_WAKEUP_PIN)), m_last_x(0), m_last_y(0), m_last_z(0) {
	DEBUG_TRACE("BMX160::BMX160");
}

double BMX160::read(unsigned int offset) {
	switch(offset) {
	case 0:
		return m_bmx160.read_temperature();
		break;
	case 1: // x
		m_bmx160.read_xyz(m_last_x, m_last_y, m_last_z);
		return m_last_x;
		break;
	case 2: // y
		return m_last_y;
		break;
	case 3: // z
		return m_last_z;
		break;
	case 4: // IRQ pending
		return (double)m_bmx160.check_and_clear_wakeup();
		break;
	default:
		return 0;
	}
}

void BMX160::calibration_write(const double value, const unsigned int offset) {
	DEBUG_TRACE("BMX160::calibrate: value=%f offset=%u", value, offset);
	if (0 == offset) {
		m_bmx160.set_wakeup_threshold(value);
	} else if (1 == offset) {
		m_bmx160.set_wakeup_duration(value);
	}
}

void BMX160::install_event_handler(unsigned int, std::function<void()> handler) {
	m_bmx160.enable_wakeup(handler);
};

void BMX160::remove_event_handler(unsigned int) {
	m_bmx160.disable_wakeup();
};
