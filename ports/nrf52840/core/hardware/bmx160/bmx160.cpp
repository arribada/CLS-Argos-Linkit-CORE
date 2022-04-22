#include <map>

#include "bmx160.h"
#include "bmx160.hpp"

#include "debug.hpp"
#include "nrf_delay.h"
#include "bsp.hpp"
#include "nrfx_twim.h"
#include "pmu.hpp"
#include "error.hpp"

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

BMX160LL::BMX160LL(unsigned int bus, unsigned char addr) : m_bus(bus), m_addr(addr), m_unique_id(BMX160LLManager::register_device(*this)) {
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

    DEBUG_TRACE("BMX160LL::read_xyz: xyz=%d,%d,%d", (int)data.x, (int)data.y, (int)data.z);

    // Convert to double precision G-force result on each axis
    x = convert_g_force(16, data.x);
    y = convert_g_force(16, data.y);
    z = convert_g_force(16, data.z);

    // Turn accelerometer on so AXL is updated
    m_bmx160_dev.accel_cfg.power = BMX160_ACCEL_SUSPEND_MODE;
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

BMX160::BMX160() : Sensor("AXL"), m_bmx160(BMX160LL(BMX160_DEVICE, BMX160_ADDRESS)), m_last_x(0), m_last_y(0), m_last_z(0) {
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
	default:
		return 0;
	}
}

void BMX160::calibrate(double, unsigned int) {
}
