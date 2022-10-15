#ifndef OTP_H
#define OTP_H

#include <stdint.h>

struct chip_info {
    uint16_t chip_id;
    uint8_t chip_revision;
    uint8_t customer_id;
    uint16_t project_id;
    uint16_t svn_revision;
    uint16_t config_id;
    uint16_t pe_id;
    uint16_t patch_id;
    uint8_t cut_id;    
};

#define SYSREG_RESET_REG        	0x2001C00C
#define SYSREG_HW_CUT_ID_REG    	0x2001C002
#define OTP_DATA_START_ADDR     	0x000600C0
#define BOOT_SET_REG            	0x2001C218
#define ILOAD_DRIVE_REG	        	0x2001C176
#define RAM_FW_START_ADDRESS    	0x00050000
#define RAM_DATA_START_ADDRESS  	0x00051000
#define FW_I2C_REG              	0x2001C014
#define FW_VERSION_HW_REG           0x2001C004

#define OPERATION_MODE_REG      	0x000E
#define OTP_WRITE_LENGTH_REG    	0x014C
#define OTP_WRITE_COMMAND_REG   	0x0145
#define FW_CLEAN_ADDRRESS_REG   	0x0146
#define GPIO_STATUS_REG         	0x0012
#define SYSTEM_ERROR_REG        	0x001D
#define WLC_CHIPID_REG      		0x0000
#define FW_VERSION_REG          	0x0006


#define FW_OP_MODE_SA           	0x01
#define SYSTEM_RESET_VALUE      	0x03
#define BOOT_FROM_RAM           	0x01
#define BOOT_FROM_ROM           	0x00
#define ILOAD_DISABLE_VALUE     	0x01
#define DISABLE_FW_I2C          	0x01
#define M0_HOLD                 	0x02
#define CLEAN_ADDR_UPDATE_VALUE 	0x40
#define GPIO_RESET_VALUE        	0x10
#define OTP_ENABLE              	0x01

#define POLLING_INTERVAL         	50
#define OTP_FLAG_POLLING_TIMEOUT 	200
#define OPCODE		            	0xFA
#define OTP_MAX_SIZE            	(16 * 1024)
#define GENERAL_WAITTIME         	100
#define HW_I2C_CHUNK_SIZE           128 
#define RAM_WRITE_BLOCK_SIZE        4096
#define RAM_SIZE                    (8 * 1024)

#define MAX_RETRY                    3

#define OK                           ((int)0x00000000)
#define E_BUS_R                      ((int)0x80000001)
#define E_BUS_W                      ((int)0x80000002)
#define E_BUS_WR                     ((int)0x80000003)
#define E_UNEXPECTED_OP_MODE         ((int)0x80000004)
#define E_SYSTEM_RESET               ((int)0x80000005)
#define E_MEMORY_ALLOC               ((int)0x80000006)
#define E_UNEXPECTED_HW_REV          ((int)0x80000007)
#define E_CFG_PATCH_ID_MISMATCH      ((int)0x80000008)
#define E_OTP_SYSTEM_CORRUPTION      ((int)0x80000009)
#define E_RAM_FW_WRITE               ((int)0x8000000A)
#define E_OTP_WRITE_TIMEOUT          ((int)0x8000000B)
#define E_RAM_DATA_WRITE             ((int)0x8000000C)
#define E_UNEXPECTED_OTP_STATUS      ((int)0x8000000D)

int otp_program(void);
int get_chip_info(struct chip_info *info);


#endif /* OTP_H */
