#include <map>

#include "bma400.h"
#include "bma400.hpp"

#include "debug.hpp"
#include "nrf_delay.h"
#include "bsp.hpp"
#include "nrfx_twim.h"
#include "pmu.hpp"
#include "error.hpp"
#include "nrf_irq.hpp"

//Todo: I cannot see the point of this unless there is more than 1 device connected to it. 
// unless it is only so that i2c_read and i2c_write works publicly
class BMA400LLManager {
private:
	static inline uint8_t m_unique_id = 0;  // default = 0
	static inline std::map<uint8_t, BMA400LL&> m_map;

public:
	static uint8_t register_device(BMA400LL& dev);
	static void unregister_device(uint8_t unique_id);
	static BMA400LL& lookup_device(uint8_t unique_id);
};

BMA400LL& BMA400LLManager::lookup_device(uint8_t unique_id)
{
	return m_map.at(unique_id);
}

uint8_t BMA400LLManager::register_device(BMA400LL& device)
{
	m_map.insert({m_unique_id, device});
	return m_unique_id++;
}

void BMA400LLManager::unregister_device(uint8_t unique_id)
{
	m_map.erase(unique_id);
}

BMA400LL::BMA400LL(unsigned int bus, unsigned char addr, int wakeup_pin) : m_bus(bus), m_addr(addr), m_irq(NrfIRQ(wakeup_pin)),
		m_unique_id(BMA400LLManager::register_device(*this)), m_accel_sleep_mode(BMA400_MODE_SLEEP),
		m_irq_pending(false)
{
	try {
		init();
	} catch(...) {
		BMA400LLManager::unregister_device(m_unique_id);
		throw;
	}
}

BMA400LL::~BMA400LL() {
	BMA400LLManager::unregister_device(m_unique_id);
}

void BMA400LL::init()
{
    int8_t rslt;

	DEBUG_TRACE("BMA400LL::init");
    
    // m_bma400_dev.id = m_unique_id; //?
    m_bma400_dev.chip_id = m_unique_id;
    m_bma400_dev.intf = BMA400_I2C_INTF;

    m_bma400_dev.write = i2c_write;
    m_bma400_dev.read = i2c_read;
    m_bma400_dev.delay_us = delay_us;

    /*! Read write length varies based on user requirement */
    #define READ_WRITE_LENGTH  UINT8_C(46) // todo: get the required read-write length
    m_bma400_dev.read_write_len = READ_WRITE_LENGTH;

    int8_t err_code;

    err_code = bma400_init(&m_bma400_dev);

    if (err_code != BMA400_OK)
    {
        DEBUG_ERROR("BMA400 initialization failure (%d)",err_code);
        throw ErrorCode::I2C_COMMS_ERROR;
    }
    // rslt = bma400_soft_reset(&m_bma400_dev);
    // bma400_check_rslt("bma400_soft_reset", rslt);

    // rslt = bma400_init(&m_bma400_dev);
    // bma400_check_rslt("bma400_init", rslt);

    /* Get the accelerometer configurations which are set in the sensor */
    rslt = bma400_get_sensor_conf(&m_bma400_sensor_conf, 1, &m_bma400_dev);
    bma400_check_rslt("bma400_get_sensor_conf", rslt);

    /* Select the type of configuration to be modified */
    m_bma400_sensor_conf.type = BMA400_ACCEL;
    /* Modify the desired configurations as per macros
     * available in bma400_defs.h file */
    m_bma400_sensor_conf.param.accel.odr = BMA400_ODR_100HZ;
    m_bma400_sensor_conf.param.accel.range = BMA400_RANGE_16G;
    m_bma400_sensor_conf.param.accel.data_src = BMA400_DATA_SRC_ACCEL_FILT_2;

    /* Set the desired configurations to the sensor */
    rslt = bma400_set_sensor_conf(&m_bma400_sensor_conf, 1, &m_bma400_dev);
    bma400_check_rslt("bma400_set_sensor_conf", rslt);

    // note: Sleep mode: Registers readable and writable, no sensortime
    rslt = bma400_set_power_mode(BMA400_MODE_SLEEP, &m_bma400_dev); 
    bma400_check_rslt("bma400_set_power_mode", rslt);


    m_bma400_device_conf.type = BMA400_INT_PIN_CONF;
    m_bma400_device_conf.param.int_conf.int_chan = BMA400_INT_CHANNEL_1;
    m_bma400_device_conf.param.int_conf.pin_conf = BMA400_INT_PUSH_PULL_ACTIVE_0;
    rslt = bma400_set_device_conf(&m_bma400_device_conf, BMA400_INT_PIN_CONF, &m_bma400_dev);
    bma400_check_rslt("bma400_set_device_conf", rslt);
}
//                       
int8_t BMA400LL::i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    nrfx_err_t err_code;
    BMA400LL& device = BMA400LLManager::lookup_device(0); //todo  Define intf_ptr to include chip_id

    if (!length)
        return BMA400_OK;
    
    if (length > BMA400_MAX_BUFFER_LEN + sizeof(reg_addr))
        return BMA400_E_OUT_OF_RANGE;
    
    uint8_t buffer[BMA400_MAX_BUFFER_LEN];
    buffer[0] = reg_addr;
    memcpy(&buffer[1], reg_data, length);
    
    nrfx_twim_xfer_desc_t xfer = NRFX_TWIM_XFER_DESC_TX(device.m_addr, (uint8_t*)buffer, length + sizeof(reg_addr));

    err_code = nrfx_twim_xfer(&BSP::I2C_Inits[device.m_bus].twim, &xfer, 0);
    //todo: what does this need?
//    err_code = nrfx_twim_tx(&BSP::I2C_Inits[device.m_bus].twim, device.m_addr, buffer, length + sizeof(reg_addr), false);
    if (err_code != NRFX_SUCCESS)
        return BMA400_E_COM_FAIL;

    return BMA400_OK;
}

int8_t BMA400LL::i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    nrfx_err_t err_code;
    BMA400LL& device = BMA400LLManager::lookup_device(0); //todo  Define intf_ptr to include chip_id

    nrfx_twim_xfer_desc_t xfer = NRFX_TWIM_XFER_DESC_TXRX(device.m_addr, &reg_addr, sizeof(reg_addr), (uint8_t*)reg_data, length);

    err_code = nrfx_twim_xfer(&BSP::I2C_Inits[device.m_bus].twim, &xfer, 0);

    // err_code = nrfx_twim_tx(&BSP::I2C_Inits[device.m_bus].twim, device.m_addr, &reg_addr, sizeof(reg_addr), true);
    // if (err_code != NRFX_SUCCESS)
    //     return BMA400_E_COM_FAIL;
    
    // err_code = nrfx_twim_rx(&BSP::I2C_Inits[device.m_bus].twim, device.m_addr, reg_data, length);
    if (err_code != NRFX_SUCCESS)
        return BMA400_E_COM_FAIL;

    return BMA400_OK;
}

void BMA400LL::delay_us(uint32_t period, void *intf_ptr)
{
    nrf_delay_us(period);
    // intf_ptr(); // this is the callback function for every time the delay function is called. 
}

double BMA400LL::convert_g_force(unsigned int g_scale, int16_t axis_value)
{
	return (double)g_scale * axis_value / 32768;
}

void BMA400LL::bma400_check_rslt(const char api_name[], int8_t rslt)
{
    switch (rslt)
    {
        case BMA400_OK:
            /* Do nothing */
            break;
        case BMA400_E_NULL_PTR:
            DEBUG_ERROR("BMA400 Error [%d] : Null pointer\r\n", rslt);
            throw ErrorCode::I2C_COMMS_ERROR;
            break;
        case BMA400_E_COM_FAIL:
            DEBUG_ERROR("BMA400 Error [%d] : Communication failure\r\n", rslt);
            throw ErrorCode::I2C_COMMS_ERROR;
            break;
        case BMA400_E_INVALID_CONFIG:
            DEBUG_ERROR("BMA400 Error [%d] : Invalid configuration\r\n", rslt);
            throw ErrorCode::I2C_COMMS_ERROR;
            break;
        case BMA400_E_DEV_NOT_FOUND:
            DEBUG_ERROR("BMA400 Error [%d] : Device not found\r\n", rslt);
            throw ErrorCode::I2C_COMMS_ERROR;
            break;
        default:
            DEBUG_ERROR("BMA400 Error [%d] : Unknown error code\r\n", rslt);
            throw ErrorCode::I2C_COMMS_ERROR;
            break;
    }

}

//todo: this is where I should read 128 samples at 20Hz so -> PMU:delay_ms()
void BMA400LL::read_xyz(double& x, double& y, double& z)
{
    int8_t rslt;
    // Turn accelerometer on so AXL is updated
    rslt = bma400_set_power_mode(BMA400_MODE_NORMAL, &m_bma400_dev);
    bma400_check_rslt("BMA400LL::read_xyz() bma400_set_power_mode normal", rslt);

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
    rslt = bma400_get_regs(BMA400_REG_ACCEL_DATA, data.buffer, sizeof(data.buffer), &m_bma400_dev);
    bma400_check_rslt("BMA400LL::read_xyz() bma400_get_regs", rslt);

    // Convert to double precision G-force result on each axis
    x = convert_g_force(16, data.x);
    y = convert_g_force(16, data.y);
    z = convert_g_force(16, data.z);

    DEBUG_TRACE("BMA400LL::read_xyz: xyz=%f,%f,%f", x, y, z);

    // Turn accelerometer on so AXL is updated
    rslt = bma400_set_power_mode(BMA400_MODE_SLEEP, &m_bma400_dev);
    bma400_check_rslt("BMA400LL::read_xyz() bma400_set_power_mode sleep", rslt);
}

double BMA400LL::read_temperature()
{
    int8_t rslt;

    // Turn accel on so temperature is updated
    rslt = bma400_set_power_mode(BMA400_MODE_NORMAL, &m_bma400_dev);
    bma400_check_rslt("BMA400LL::read_temperature() bma400_set_power_mode normal", rslt);

    // Wait 10ms +/-12% for temperature reading
    PMU::delay_us(11200);

    // Read temperature
    int16_t temperature_data;
    bma400_get_temperature_data(&temperature_data, &m_bma400_dev);
    bma400_check_rslt("BMA400LL::read_temperature() bma400_get_temperature_data", rslt);

    // temperature_data = 195 ---> 19,5 degrees Celsius.

    DEBUG_TRACE("BMA400LL::read_temperature: temperature_data=%ld", temperature_data);
    // Sleep mode
    bma400_set_power_mode(BMA400_MODE_SLEEP, &m_bma400_dev);
    bma400_check_rslt("BMA400LL::read_temperature() bma400_set_power_mode sleep", rslt);

    return temperature_data;
}

void BMA400LL::set_wakeup_threshold(double thresh)
{
	m_wakeup_threshold = thresh;
}

void BMA400LL::set_wakeup_duration(double duration)
{
	m_wakeup_duration = duration;
}

void BMA400LL::enable_wakeup(std::function<void()> func)
{
    int8_t rslt;

	// Enable IRQ
	bma400_set_power_mode(BMA400_MODE_NORMAL, &m_bma400_dev);
	// FIXME - minimum threshold of 2.4g??? bmx160 was 7.9g
    uint8_t anymotion_thr = (uint8_t)(std::max(255.0, (m_wakeup_threshold) * 1000 / 8.0)); // todo: this max doesn't seem right


    m_bma400_sensor_conf.type = BMA400_GEN1_INT;
    m_bma400_sensor_conf.param.gen_int = 
	{
		.gen_int_thres = anymotion_thr,
		.gen_int_dur = (uint8_t)(m_wakeup_duration - 1),
		.axes_sel = BMA400_AXIS_XYZ_EN,
		.data_src = BMA400_DATA_SRC_ACC_FILT2,
		.criterion_sel = BMA400_ACTIVITY_INT,
		.evaluate_axes = BMA400_ANY_AXES_INT,
		.ref_update = BMA400_UPDATE_EVERY_TIME,
		.hysteresis = BMA400_HYST_96_MG,
        .int_thres_ref_x = 0, // not used
        .int_thres_ref_y = 0, // not used
        .int_thres_ref_z = 0, // not used
        .int_chan = BMA400_INT_CHANNEL_1
	};
    
    /* Set the desired configurations to the sensor */
    rslt = bma400_set_sensor_conf(&m_bma400_sensor_conf, 1, &m_bma400_dev);    
    bma400_check_rslt("set_gen1_int", rslt);

	// Enable Generic interrupt 1
	m_bma400_int_en.type = BMA400_GEN1_INT_EN;
	m_bma400_int_en.conf = BMA400_ENABLE;
	rslt = bma400_enable_interrupt(&m_bma400_int_en, 1, &m_bma400_dev);
    bma400_check_rslt("bma400_enable_interrupt", rslt);

	m_irq.enable([this, func]() {
		if (!m_irq_pending) {
			m_irq_pending = true;
			func();
		}
	});
}

void BMA400LL::disable_wakeup()
{
    int8_t rslt;

	// Disable IRQ
	m_irq.disable();
	bma400_set_power_mode(BMA400_MODE_SLEEP, &m_bma400_dev);

	// Disable Generic interrupt 1
	m_bma400_int_en.type = BMA400_GEN1_INT_EN;
	m_bma400_int_en.conf = BMA400_DISABLE;
	rslt = bma400_enable_interrupt(&m_bma400_int_en, 1, &m_bma400_dev);
    bma400_check_rslt("bma400_enable_interrupt", rslt);

	m_irq.disable();
}

bool BMA400LL::check_and_clear_wakeup()
{
	InterruptLock lock;
	bool value = m_irq_pending;
	m_irq_pending = false;
	DEBUG_TRACE("BMA400LL::check_and_clear_wakeup: pending=%u", (unsigned int)value);
	return value;
}

BMA400::BMA400() : Sensor("AXL"), m_bma400(BMA400LL(BMA400_DEVICE, BMA400_ADDRESS, BMA400_WAKEUP_PIN)), m_last_x(0), m_last_y(0), m_last_z(0)
{
	DEBUG_TRACE("BMA400::BMA400");
}

double BMA400::read(unsigned int offset)
{
	switch(offset) {
	case 0:
		return m_bma400.read_temperature();
		break;
	case 1: // x
		m_bma400.read_xyz(m_last_x, m_last_y, m_last_z);
		return m_last_x;
		break;
	case 2: // y
		return m_last_y;
		break;
	case 3: // z
		return m_last_z;
		break;
	case 4: // IRQ pending
		return (double)m_bma400.check_and_clear_wakeup();
		break;
	default:
		return 0;
	}
}

void BMA400::calibration_write(const double value, const unsigned int offset)
{
	DEBUG_TRACE("BMA400::calibrate: value=%f offset=%u", value, offset);
	if (0 == offset) {
		m_bma400.set_wakeup_threshold(value);
	} else if (1 == offset) {
		m_bma400.set_wakeup_duration(value);
	}
}

void BMA400::install_event_handler(unsigned int, std::function<void()> handler)
{
	m_bma400.enable_wakeup(handler);
};

void BMA400::remove_event_handler(unsigned int)
{
	m_bma400.disable_wakeup();
};
