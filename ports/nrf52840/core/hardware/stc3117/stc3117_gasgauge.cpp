
#include <stdint.h>

#include "stc3117_gasgauge.hpp"
extern "C" {
	#include "Generic_I2C.h"
 	#include "stc311x_BatteryInfo.h"
	#include "stc311x_gasgauge.h"
}
#include "bsp.hpp"
#include "error.hpp"
#include "debug.hpp"
#include "nrf_delay.h"
#include "error.hpp"

#include "nrf_i2c.hpp"
//#include "nrfx_twim.h"
#ifndef CPPUTEST
#include "crc16.h"
#else
#define crc16_compute(x, y, z)  0xFFFF
#endif

// Thresholds for low/critical battery filtering
#define CRITICIAL_V_THRESHOLD_MV	250
#define LOW_BATT_THRESHOLD			5


static void GasGauge_DefaultInit(GasGauge_DataTypeDef * GG_struct);

// These filtered values should be retained in noinit RAM so they can survive a reset
static __attribute__((section(".noinit"))) volatile uint16_t m_filtered_values[2];
static __attribute__((section(".noinit"))) volatile uint16_t m_crc;

// Function to register the I2C functions
static void setup_i2c() {
    RegisterI2C_Write(GaugeBatteryMonitor::i2c_write);
    RegisterI2C_Read(GaugeBatteryMonitor::i2c_read);
}


GaugeBatteryMonitor::GaugeBatteryMonitor(uint16_t critical_voltage,
		uint8_t low_level
		) :
		BatteryMonitor(low_level, critical_voltage)
{
    DEBUG_TRACE("GasGaugeBatteryMonitor init...");

    // Register the I2C functions with the C driver
    setup_i2c();  // This ensures I2C functions are registered

    DEBUG_TRACE("I2C functions attributed");

    int status = check_i2c_device();
    DEBUG_TRACE("I2C device checked");
    if (status >= 0) 
    {
        //Config init
        GasGauge_DefaultInit(&STC3117_GG_struct);

        //Call STC311x driver START initialization function
        status = GasGauge_Start(&STC3117_GG_struct);

        if(status!=0 && status!=-2)
        {
            DEBUG_ERROR("Error in GasGauge_Start\n");
            status = -1; //return on error
            m_is_init = false;
        } else 
        {
            m_is_init = true;
        }
    }
}

// Forwarding C++ I2C functions to the C driver
int GaugeBatteryMonitor::i2c_write(int I2cSlaveAddr, int RegAddress, unsigned char* TxBuffer, int NumberOfBytes) 
{
	uint8_t reg_addr = static_cast<uint8_t>(RegAddress);

    if (!NumberOfBytes)
	{
        return STC3117_OK;
	}
    
	if (NumberOfBytes > static_cast<int>(STC3117_MAX_BUFFER_LEN + sizeof(reg_addr)))
	{
        return STC3117_E_OUT_OF_RANGE;
	}
    
    uint8_t buffer[STC3117_MAX_BUFFER_LEN];
    buffer[0] = reg_addr;
    memcpy(&buffer[1], TxBuffer, NumberOfBytes);

    NrfI2C::write(STC3117_DEVICE, I2cSlaveAddr, buffer, NumberOfBytes + sizeof(reg_addr), false);
/*     if (err_code != NRFX_SUCCESS)
	{
        return STC3117_E_COM_FAIL;
	}
 */
    return STC3117_OK;
}

int GaugeBatteryMonitor::i2c_read(int I2cSlaveAddr, int RegAddress, unsigned char* RxBuffer, int NumberOfBytes) 
{
	uint8_t reg_addr = static_cast<uint8_t>(RegAddress);
    NrfI2C::write(STC3117_DEVICE, I2cSlaveAddr, &reg_addr, sizeof(reg_addr), true);
    NrfI2C::read(STC3117_DEVICE, I2cSlaveAddr, RxBuffer, NumberOfBytes);

    return STC3117_OK;
}
int GaugeBatteryMonitor::check_i2c_device() {
   	int status = STC31xx_CheckI2cDeviceId();
	if (status != 0) //error
	{
		DEBUG_ERROR("STC3117: I2C error\n");
		if(status == -1)
			DEBUG_ERROR("STC3117: I2C error\n");
		else if(status == -2)
			DEBUG_ERROR("STC3117: Wrong device detected\n");
		else
			DEBUG_ERROR("STC3117: Unknown Hardware error\n");
		
		return status;
	}
    DEBUG_INFO("STC3117: Gas Gauge device found\n");
    
    int CounterValue = 0;
	for(int i=0; i<20; i++)  //check for 20*100ms = 2s
	{
		CounterValue = STC31xx_GetRunningCounter();
		if(CounterValue >= 3) //ok, device ready
		{
			break; //exit loop
		}
		else if(CounterValue < 0) //communication Error
		{
			DEBUG_ERROR("STC3117: Error at power up.\n");
			status = -1;
		}
		else
		{
			//wait for battery measurement
			nrf_delay_ms(100);
		}
	}

	if(CounterValue < 3) //timeout, the devise has not started
	{
		DEBUG_ERROR("STC3117 timeout: Error at power up.\n");
	}

    return status;
}

void GaugeBatteryMonitor::internal_update() {
	// Sample ADC and convert values
    //Call task function	
    int Voltage;
	int Soc;
	//int Current;
    int status = GasGauge_Task(&STC3117_GG_struct);  /* process gas gauge algorithm, returns results */
	uint16_t mv = 0;
	uint8_t level = 0;
    if (status > 0) //OK, new data available
    {
        /* results available */
        Soc = STC3117_GG_struct.SOC;
        Voltage = STC3117_GG_struct.Voltage;
        //Current = STC3117_GG_struct.Current;

		mv = (uint16_t)Voltage;
		level = (uint8_t)(Soc / 10);

        DEBUG_INFO("Vbat: %i mV, I=%i mA SoC=%i, C=%i", 
            STC3117_GG_struct.Voltage, 
            STC3117_GG_struct.Current, 
            STC3117_GG_struct.SOC, 
            STC3117_GG_struct.ChargeValue);
    }
    else if(status == 0) //only previous SOC, OCV and voltage are valid
    {
        DEBUG_INFO("Previous_SoC=%i, OCV=%i \r\n", 
            STC3117_GG_struct.SOC, 
            STC3117_GG_struct.OCV);
		mv = (uint16_t)STC3117_GG_struct.Voltage;
		level = (uint8_t)(STC3117_GG_struct.SOC / 10);
		
    }
    else if(status == -1) //error occurred
    {
        /* results available */
        //Soc = (STC3117_GG_struct.SOC+5)/10;
        //Voltage = STC3117_GG_struct.Voltage;
    }

	// Check CRC of the previously stored filtered values
	uint16_t crc = crc16_compute((const uint8_t *)m_filtered_values, sizeof(m_filtered_values), nullptr);
	if (crc == m_crc) {
		// Previously filtered values are valid -- make sure we don't
		// allow filtered values to bounce around the threshold margin
		// thus causing events to re-trigger unless a sufficient exit
		// threshold is reached
		if (m_filtered_values[0] < m_critical_voltage_mv) {
			if (mv >= (m_critical_voltage_mv + CRITICIAL_V_THRESHOLD_MV))
				m_filtered_values[0] = mv;
		} else {
			m_filtered_values[0] = mv;
		}
		if (m_filtered_values[1] < m_low_level) {
			if (level >= (m_low_level + LOW_BATT_THRESHOLD))
				m_filtered_values[1] = level;
		} else {
			m_filtered_values[1] = level;
		}
	} else {
		// No previous values so set new values
		m_filtered_values[0] = mv;
		m_filtered_values[1] = level;
	}

	// Updated CRC in noinit RAM
	m_crc = crc16_compute((const uint8_t *)m_filtered_values, sizeof(m_filtered_values), nullptr);

	// Apply new values
	m_last_voltage_mv = mv;
	m_last_level = level;

	// Set flags
	m_is_critical_voltage = m_filtered_values[0] < m_critical_voltage_mv;
	m_is_low_level = m_filtered_values[1] < m_low_level;
}

static void GasGauge_DefaultInit(GasGauge_DataTypeDef * GG_struct)
{
	int Rint;


	//structure initialisation

	GG_struct->Cnom = BATT_CAPACITY;        /* nominal Battery capacity in mAh */  //Warning: Battery dependant. Put the corresponding used value.
	
	GG_struct->Vmode = MONITORING_MODE;       /* 1=Voltage mode, 0=mixed mode */
	

	Rint = BATT_RINT;
	if (Rint == 0)  Rint = 200; //force default

	GG_struct->VM_cnf = (int) ((Rint * BATT_CAPACITY) / 977.78);       /* nominal VM cnf */


	if (MONITORING_MODE == MIXED_MODE)
	{
		GG_struct->CC_cnf = (int)((RSENSE * BATT_CAPACITY) / 49.556);     /* nominal CC_cnf, for CC mode only */
	}


	GG_struct->SoctabValue[0] = 0;        /* SOC curve adjustment = 0%*/
	GG_struct->SoctabValue[1] = 3 * 2;    /* SOC curve adjustment = 3%*/
	GG_struct->SoctabValue[2] = 6 * 2;    /* SOC curve adjustment = 6%*/
	GG_struct->SoctabValue[3] = 10 * 2;    /* SOC curve adjustment = 10%*/
	GG_struct->SoctabValue[4] = 15 * 2;    /* SOC curve adjustment = 15%*/
	GG_struct->SoctabValue[5] = 20 * 2;    /* SOC curve adjustment = 20%*/
	GG_struct->SoctabValue[6] = 25 * 2;    /* SOC curve adjustment = 25%*/
	GG_struct->SoctabValue[7] = 30 * 2;    /* SOC curve adjustment = 30%*/
	GG_struct->SoctabValue[8] = 40 * 2;    /* SOC curve adjustment = 40%*/
	GG_struct->SoctabValue[9] = 50 * 2;    /* SOC curve adjustment = 50%*/
	GG_struct->SoctabValue[10] = 60 * 2;    /* SOC curve adjustment = 60%*/
	GG_struct->SoctabValue[11] = 65 * 2;    /* SOC curve adjustment = 65%*/
	GG_struct->SoctabValue[12] = 70 * 2;    /* SOC curve adjustment = 70%*/
	GG_struct->SoctabValue[13] = 80 * 2;    /* SOC curve adjustment = 80%*/
	GG_struct->SoctabValue[14] = 90 * 2;    /* SOC curve adjustment = 90%*/
	GG_struct->SoctabValue[15] = 100 * 2;    /* SOC curve adjustment = 100%*/


#ifdef	DEFAULT_BATTERY_4V20_MAX      //Default OCV curve for a 4.20V max battery
	GG_struct->OcvValue[0] = 0x1770;    /* OCV curve value at 0%*/
	GG_struct->OcvValue[1] = 0x1926;    /* OCV curve value at 3%*/
	GG_struct->OcvValue[2] = 0x19B2;    /* OCV curve value at 6%*/
	GG_struct->OcvValue[3] = 0x19FB;    /* OCV curve value at 10%*/
	GG_struct->OcvValue[4] = 0x1A3E;    /* OCV curve value at 15%*/
	GG_struct->OcvValue[5] = 0x1A6D;    /* OCV curve value at 20%*/
	GG_struct->OcvValue[6] = 0x1A9D;    /* OCV curve value at 25%*/
	GG_struct->OcvValue[7] = 0x1AB6;    /* OCV curve value at 30%*/
	GG_struct->OcvValue[8] = 0x1AD5;    /* OCV curve value at 40%*/
	GG_struct->OcvValue[9] = 0x1B01;    /* OCV curve value at 50%*/
	GG_struct->OcvValue[10] = 0x1B70;    /* OCV curve value at 60%*/
	GG_struct->OcvValue[11] = 0x1BB1;    /* OCV curve value at 65%*/
	GG_struct->OcvValue[12] = 0x1BE8;    /* OCV curve value at 70%*/
	GG_struct->OcvValue[13] = 0x1C58;    /* OCV curve value at 80%*/
	GG_struct->OcvValue[14] = 0x1CF3;    /* OCV curve value at 90%*/
	GG_struct->OcvValue[15] = 0x1DA9;    /* OCV curve value at 100%*/
#endif

#ifdef	DEFAULT_BATTERY_4V35_MAX      //Default OCV curve for a 4.35V max battery
	GG_struct->OcvValue[0] = 0x1770;    /* OCV curve value at 0%*/
	GG_struct->OcvValue[1] = 0x195D;    /* OCV curve value at 3%*/
	GG_struct->OcvValue[2] = 0x19EE;    /* OCV curve value at 6%*/
	GG_struct->OcvValue[3] = 0x1A1A;    /* OCV curve value at 10%*/
	GG_struct->OcvValue[4] = 0x1A59;    /* OCV curve value at 15%*/
	GG_struct->OcvValue[5] = 0x1A95;    /* OCV curve value at 20%*/
	GG_struct->OcvValue[6] = 0x1AB6;    /* OCV curve value at 25%*/
	GG_struct->OcvValue[7] = 0x1AC7;    /* OCV curve value at 30%*/
	GG_struct->OcvValue[8] = 0x1AEB;    /* OCV curve value at 40%*/
	GG_struct->OcvValue[9] = 0x1B2B;    /* OCV curve value at 50%*/
	GG_struct->OcvValue[10] = 0x1BCC;    /* OCV curve value at 60%*/
	GG_struct->OcvValue[11] = 0x1C13;    /* OCV curve value at 65%*/
	GG_struct->OcvValue[12] = 0x1C57;    /* OCV curve value at 70%*/
	GG_struct->OcvValue[13] = 0x1D09;    /* OCV curve value at 80%*/
	GG_struct->OcvValue[14] = 0x1DCF;    /* OCV curve value at 90%*/
	GG_struct->OcvValue[15] = 0x1EA2;    /* OCV curve value at 100%*/
#endif

#ifdef	CUSTOM_BATTERY_OCV   //fill custom battery data
	GG_struct->OcvValue[0] = ...;
	GG_struct->OcvValue[1] = ...;
	//...
	//...
#endif

	GG_struct->CapDerating[6]=0;   /* capacity derating in 0.1%, for temp = -20 �C */
	GG_struct->CapDerating[5]=0;   /* capacity derating in 0.1%, for temp = -10 �C */
	GG_struct->CapDerating[4]=0;   /* capacity derating in 0.1%, for temp = 0   �C */
	GG_struct->CapDerating[3]=0;   /* capacity derating in 0.1%, for temp = 10  �C */
	GG_struct->CapDerating[2]=0;   /* capacity derating in 0.1%, for temp = 25  �C */
	GG_struct->CapDerating[1]=0;   /* capacity derating in 0.1%, for temp = 40  �C */
	GG_struct->CapDerating[0]=0;   /* capacity derating in 0.1%, for temp = 60  �C */


	

	GG_struct->Alm_SOC = 10;     /* SOC alm level % */
	GG_struct->Alm_Vbat = 3600;    /* Vbat alm level mV */


	GG_struct->Rsense = RSENSE;      /* sense resistor mOhms */   //Warning: Hardware dependant. Put the corresponding used value
	GG_struct->RelaxCurrent = GG_struct->Cnom/20;  /* current for relaxation (< C/20) mA */

	GG_struct->ForceExternalTemperature = 0; //0: do not force Temperature but Gas gauge measures it

}
