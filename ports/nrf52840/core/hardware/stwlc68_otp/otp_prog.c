#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "otp.h"
#include "otp_data.h"
#include "fw.h"
#include "otphal.h"

static int hw_i2c_write(uint32_t addr, uint8_t* data, int data_length)
{
    uint8_t* cmd = malloc((5 + data_length) * sizeof(uint8_t));    

    cmd[0] = OPCODE;
    cmd[1] = (uint8_t)((addr >> 24) & 0xFF);
    cmd[2] = (uint8_t)((addr >> 16) & 0xFF);
    cmd[3] = (uint8_t)((addr >> 8) & 0xFF);
    cmd[4] = (uint8_t)((addr >> 0) & 0xFF);
    
    memcpy(&cmd[5], data, data_length);

    if ((otphal_i2c_write(cmd, 5 + data_length)) < OK) {
        printf("Error writing hardware register\n");
        free(cmd);
        return E_BUS_W;
    }

    free(cmd);
    return OK;
}

static int fw_i2c_write(uint16_t addr, uint8_t* data, uint32_t data_length)
{
    uint8_t* cmd = malloc((2 + data_length) * sizeof(uint8_t));     

    cmd[0] = (uint8_t)((addr >>  8) & 0xFF);
    cmd[1] = (uint8_t)((addr >>  0) & 0xFF);
    
    memcpy(&cmd[2], data, data_length);
        
    if((otphal_i2c_write(cmd, (2 + data_length))) < OK) {
        printf("Error writing firmware register\n");
        free(cmd);
        return E_BUS_W;
    } 
          
    free(cmd);
    return OK;
}

static int hw_i2c_read(uint32_t addr, uint8_t* read_buff, int read_count)
{
    uint8_t cmd[5];     

    cmd[0] = OPCODE;
    cmd[1] = (uint8_t)((addr >> 24) & 0xFF);
    cmd[2] = (uint8_t)((addr >> 16) & 0xFF);
    cmd[3] = (uint8_t)((addr >>  8) & 0xFF);
    cmd[4] = (uint8_t)((addr >>  0) & 0xFF);

    if((otphal_i2c_write_read(cmd, 5 , read_buff, read_count)) < OK) {
        printf("Error reading the hardware register\n");
        return E_BUS_WR;
    }

    return OK;
}

static int fw_i2c_read(uint16_t addr, uint8_t* read_buff, int read_count)
{
    uint8_t cmd[2];

    cmd[0] = (uint8_t)((addr >>  8) & 0xFF);
    cmd[1] = (uint8_t)((addr >>  0) & 0xFF);
    
    if((otphal_i2c_write_read(cmd, 2, read_buff, read_count)) < OK) {
        printf("Error reading the firmware register\n");
        return E_BUS_WR;
    }

    return OK;
}

static int system_reset()
{
    int err = 0;
    uint8_t cmd[2] = { 0xFF, 0xFF};
    
    if((err = hw_i2c_write(FW_VERSION_HW_REG , cmd, 2)) < OK) {        
        printf("Error while system resetting\n");
        return E_BUS_WR;
    }
    
    cmd[0] = SYSTEM_RESET_VALUE;
    hw_i2c_write(SYSREG_RESET_REG, cmd, 1);
    otphal_sleep_ms(GENERAL_WAITTIME);
    
    uint8_t read_buff[2] = {0x00, 0x00};
    if((err = hw_i2c_read(FW_VERSION_HW_REG , read_buff, 2)) < OK) {        
        printf("Error while system resetting\n");
        return E_BUS_WR;
    }
    
    if((read_buff[0] == 0xFF) || (read_buff[1] == 0xFF)) {
        printf("Error while system resetting\n");
        return E_BUS_W;
    }
    
    return OK;
}

static int ram_write(uint8_t* data, int data_size, int addr)
{
    int err = 0;
    int to_write = 0;
    int remaining = data_size;
    int address = addr;
    uint32_t written_already = 0;
    uint8_t *buff = NULL;
    buff = (uint8_t*)malloc(data_size * sizeof(uint8_t));
    if (buff == NULL) return E_MEMORY_ALLOC;
 
    while (remaining > 0) {
        to_write = remaining > HW_I2C_CHUNK_SIZE ? HW_I2C_CHUNK_SIZE : remaining;
        memcpy(buff, data + written_already, to_write);
        if ((err = hw_i2c_write(address, buff, to_write)) < OK) {
            free(buff);
            buff = NULL;
            return err;
        }
        address += to_write;
        written_already += to_write;
        remaining -= to_write;
    }
    
    free(buff);
    buff = NULL;
    return OK;
 }
 
static int ram_read(uint8_t* data, int data_size, int addr)
{
    int err = 0;
    int remaining = data_size;
    int to_read = 0;    
    int address = addr;
    uint32_t read_already = 0;
 
    while (remaining > 0) {        
        to_read = remaining > HW_I2C_CHUNK_SIZE ? HW_I2C_CHUNK_SIZE : remaining;
         
        if ((err = hw_i2c_read(address, data + read_already, to_read)) < OK) return E_BUS_WR;
        
        address += to_read;
        read_already += to_read;
        remaining -= to_read;
    }
 
    return OK;
}

static int load_ram_fw()
{
    int err = 0;
    int retry = 0;
    int ram_fw_data_size;
    uint8_t write_buff[1] = {0x00};
    uint8_t *ram_fw_data = NULL;
    uint8_t *ram_read_data = NULL;   
 
    write_buff[0] = DISABLE_FW_I2C;
    if((err = hw_i2c_write(FW_I2C_REG, write_buff, 1)) < OK) return err;
 
    write_buff[0] = M0_HOLD;
    if((err = hw_i2c_write(SYSREG_RESET_REG, write_buff, 1)) < OK) return err;
 
    ram_fw_data_size = FW_SIZE;
    ram_fw_data = (uint8_t*)malloc(ram_fw_data_size * sizeof(uint8_t));
 
    if (ram_fw_data == NULL) {     
        printf("Error allocating memory\n");
        return E_MEMORY_ALLOC;
    }
 
    ram_read_data = (uint8_t*)malloc(ram_fw_data_size * sizeof(uint8_t));
 
    if (ram_read_data == NULL) {     
        printf("Error allocating memory\n");
        free(ram_fw_data);
        ram_fw_data = NULL;
        return E_MEMORY_ALLOC;
    }
     
    memcpy(ram_fw_data, fw_data, ram_fw_data_size);
    printf("Ram FW size: %d bytes\n", ram_fw_data_size);
 
    while(retry < MAX_RETRY) {
        
        if((err = ram_write(ram_fw_data, ram_fw_data_size, RAM_FW_START_ADDRESS)) < OK) {
            printf("Error while loading the data to RAM\n");
            free(ram_fw_data);
            ram_fw_data = NULL;
            free(ram_read_data);
            ram_read_data = NULL;
            return E_RAM_FW_WRITE;
        }
 
        if((err = ram_read(ram_read_data, ram_fw_data_size, RAM_FW_START_ADDRESS)) < OK) {
            printf("Error while reading the data from RAM\n");
            free(ram_fw_data);
            ram_fw_data = NULL;
            free(ram_read_data);
            ram_read_data = NULL;
            return E_RAM_FW_WRITE;
        }
 
        if(memcmp(ram_fw_data, ram_read_data, ram_fw_data_size) == OK)
            break;
         
        retry++;
 
        printf("Mismatch in RAM data read back, retrying.. attempt:%d\n", retry);
    }
 
    if(retry == MAX_RETRY) {             
        printf("RAM firmware is not loaded correctly\n");
        err = E_RAM_FW_WRITE;
    }
 
    else {
        printf("RAM firmware Loaded Successfully\n");
        err = OK;
    }
 
    free(ram_fw_data);
    ram_fw_data = NULL;
    free(ram_read_data);
    ram_read_data = NULL;
    return err;
} 

static int rom_mode_boot()
{     
    uint8_t write_buff[1] = {0x00};
 
    write_buff[0] = BOOT_FROM_ROM;
    if(hw_i2c_write(BOOT_SET_REG, write_buff, 1) < OK) {
        printf("Error writing boot mode register\n");
        return E_BUS_W;
    }   
        
    if(system_reset() != OK) {
        printf("Error while system resetting \n");
        return E_SYSTEM_RESET;
    } 
    return OK;
}

static int otp_write(uint8_t* data, int data_size, int addr)
{
    int err = 0;
    int remaining = data_size;
    int to_write = 0;
    int address = addr;
    int loop = 0;
    int otp_flag_status = 0;
    int retry = 0;
    uint32_t written_already = 0;
    uint8_t write_buff[10] = {0x00};
    uint8_t *read_buff = NULL;
    uint8_t *buff = NULL;
 
    buff = (uint8_t*)malloc(data_size * sizeof(uint8_t));
    if (buff == NULL) return E_MEMORY_ALLOC;

    read_buff = (uint8_t*)malloc(data_size * sizeof(uint8_t));
    if (read_buff == NULL) {
        free(buff);
        buff = NULL;
        return E_MEMORY_ALLOC;
    }
    
    while (remaining > 0) {        
       if(otphal_gpio_get_value() == 0) {
            printf("Int pin status should be high before start programming\n");
            err = E_UNEXPECTED_OTP_STATUS;
            break;
       }   
            
        to_write = remaining > RAM_WRITE_BLOCK_SIZE ? RAM_WRITE_BLOCK_SIZE : remaining;
 
        write_buff[0] = (uint8_t)((to_write >> 0) & 0xFF);
        write_buff[1] = (uint8_t)((to_write >> 8) & 0xFF);
        
        printf("OTP Write Length %d bytes\n", ((write_buff[1] << 8) + write_buff[0]));
        if ((err = fw_i2c_write(OTP_WRITE_LENGTH_REG, write_buff, 2)) < OK) {
            printf("Error writing otp length register\n");
            break;
        }
 
        memcpy(buff, data + written_already, to_write);
         
        while(retry < MAX_RETRY) {            
            if (ram_write(buff, to_write, address) < OK) break;
 
            if (ram_read(read_buff, to_write, address) < OK) break;
 
            if(memcmp(buff, read_buff, to_write) == OK)
                break;
 
            retry++;
            printf("Mismatch in ram data read back, retrying.. attempt:%d\n", retry);
        }
        
        printf("OTP data loaded successfully in the RAM!!\n");
 
        if((retry == MAX_RETRY) || err != OK) {            
            printf("Error loading OTP data to RAM\n");
            err = E_RAM_DATA_WRITE;
            break;
        }
         
         //write OTP
        write_buff[0] = OTP_ENABLE;
        if ((err = fw_i2c_write(OTP_WRITE_COMMAND_REG, write_buff, 1)) < OK) {            
            printf("Error while triggering otp write\n");
            break;
        }
         
        written_already += to_write;
        remaining -= to_write;
        
        while(loop < OTP_FLAG_POLLING_TIMEOUT) {            
            if(otphal_gpio_get_value() == 0) {
                otp_flag_status = 1;
                loop = 0;
                break;
            }
            otphal_sleep_ms(POLLING_INTERVAL);
            loop++;
         }
 
        if(!otp_flag_status) {            
            printf("Timeout error while writing data from Ram to OTP\n");
            err = E_OTP_WRITE_TIMEOUT;  
            break;
        }
        
        write_buff[0] = GPIO_RESET_VALUE;
        if ((err = fw_i2c_write(GPIO_STATUS_REG, write_buff, 1)) < OK) {
            printf("Error while resetting the gpio\n");
            break;
        }
    }
     
    free(buff);
    buff = NULL;
    free(read_buff);
    read_buff = NULL;
    return err;
}

int otp_program(void)
{
    int err =0;  
    int clean_addr_current = OTP_DATA_START_ADDR;
    int data_to_program_size;
    int otp_free_space = 0;
    struct chip_info info;
    uint8_t read_buff[10] = {0x00};
    uint8_t write_buff[10] = {0x00};
    uint8_t dummy_header[8] = { 0xA7, 0x00, 0xFF, 0xFF, 0x58, 0xFF, 0x00, 0x00 };
    uint8_t *data_to_program = NULL;
    
    if(system_reset() != OK) return E_SYSTEM_RESET;

    if((err = fw_i2c_read(OPERATION_MODE_REG, read_buff, 1)) < OK) {
        printf("Error while reading operation mode\n");
        return err;
    }
    
    printf("Operation mode %02X\n", read_buff[0]);
    if(read_buff[0] != FW_OP_MODE_SA) {
        printf("OTP must be programmed in DC power\n");
        return E_UNEXPECTED_OP_MODE;
    }
    
    if(system_reset() != OK) return E_SYSTEM_RESET;
    
    if((err = get_chip_info(&info)) < OK) return err;
    
    printf("Cut Id: %02X\n", info.cut_id); 
    if (info.cut_id != OTP_TARGET_CUT_ID) {
        printf("HW cut id mismatch with Target cut id, OTP is not programmed \n");
        return E_UNEXPECTED_HW_REV;
    }
    
    if ((info.config_id == OTP_CFG_VERSION_ID) || (info.patch_id == OTP_PATCH_VERSION_ID)) {
        printf("Cannot program in already programmed part\n");
        return E_UNEXPECTED_OTP_STATUS;
    }
    
     if((err = fw_i2c_read(SYSTEM_ERROR_REG, read_buff, 1)) < OK) {
        printf("Error while reading otp errors\n");
        return err;
    }

    printf("OTP Error Status: %02X\n", read_buff[0]);
    if((read_buff[0] & 0xC3) != OK) {
        printf("OTP status is not good for programming\n");
        return E_UNEXPECTED_OTP_STATUS;
    }     
   
    data_to_program_size = OTP_CFG_SIZE + OTP_PATCH_SIZE + 8;
    data_to_program = (uint8_t*)malloc(data_to_program_size * sizeof(uint8_t));
    if (data_to_program == NULL) return E_MEMORY_ALLOC;
    
    memcpy(data_to_program, otp_cfg_data, OTP_CFG_SIZE);
    memcpy(data_to_program + OTP_CFG_SIZE, otp_patch_data, OTP_PATCH_SIZE);
    memcpy(data_to_program + OTP_CFG_SIZE + OTP_PATCH_SIZE, dummy_header, 8);
   
    printf("Clean Addr: %08X\n", clean_addr_current);    
    otp_free_space = OTP_MAX_SIZE - (clean_addr_current & 0xFFFF);    
    printf("OTP available space: %d bytes, data to program size: %d bytes\n", otp_free_space, data_to_program_size);
   
	write_buff[0] = (uint8_t)(clean_addr_current & 0x00FF);
	write_buff[1] = (uint8_t)((clean_addr_current & 0xFF00) >> 8);

	if ((err = fw_i2c_write(FW_CLEAN_ADDRRESS_REG, write_buff, 2)) < OK) {
		printf("Error updating the clean address\n");
		goto exit_1;
	}
	
	write_buff[0] = CLEAN_ADDR_UPDATE_VALUE;
	if ((err = fw_i2c_write(OTP_WRITE_COMMAND_REG, write_buff, 1)) < OK) {
		printf("Error updating the clean address\n");
		goto exit_1;
	}
	
    printf("Loading RAM FW!! \n");
    if((err = load_ram_fw()) < OK) goto exit_1;

    printf("Booting in RAM \n");
    write_buff[0] = BOOT_FROM_RAM;
    if((err = hw_i2c_write(BOOT_SET_REG, write_buff, 1)) < OK) {
        printf("Error booting from RAM\n");
        goto exit_1;
    }
    
    if(system_reset() != OK) {
        err = E_SYSTEM_RESET; 
        goto exit_1;
    }
     
    if((err = fw_i2c_read(FW_VERSION_REG, read_buff, 2)) < OK) {
        printf("Error reading RAM firmware version\n");
        rom_mode_boot();
        goto exit_1;
    }
     
    printf("RAM FW Version: 0x%04X\n", ((read_buff[1] << 8) + read_buff[0]));

    write_buff[0] = ILOAD_DISABLE_VALUE;
    if((err = hw_i2c_write(ILOAD_DRIVE_REG, write_buff, 1)) < OK) {
        printf("Error disabling iload\n");
		rom_mode_boot();
        goto exit_1;
    }
 
    if((err = otp_write(data_to_program, data_to_program_size, RAM_DATA_START_ADDRESS)) < OK) {
        printf("Error while writing OTP\n");
        rom_mode_boot();
        goto exit_1;
    }
    
    if((err = fw_i2c_read(SYSTEM_ERROR_REG, read_buff, 1)) < OK) {
        printf("Error while reading otp errors\n");
		rom_mode_boot();
        goto exit_1;
    }

    printf("OTP System Error Status: %02X\n", read_buff[0]);
    if(read_buff[0] != OK) {
        printf("OTP system errors latched; Error in OTP flashing\n");
        rom_mode_boot();
        err = E_OTP_SYSTEM_CORRUPTION;
        goto exit_1;
    }
 
    write_buff[0] = BOOT_FROM_ROM;
    if((err = hw_i2c_write(BOOT_SET_REG, write_buff, 1)) < OK) {
        printf("Error in writing boot mode register\n");
        goto exit_1;
    }
    
    if(system_reset() != OK) return E_SYSTEM_RESET;
 
    if((err = get_chip_info(&info)) < OK) {
        printf("Error reading device info\n");
        goto exit_1;
    }
 
    if ((info.config_id != OTP_CFG_VERSION_ID) || (info.patch_id != OTP_PATCH_VERSION_ID)) {
        printf("Config Id/Patch Id mismatch after OTP programming\n");
        err = E_CFG_PATCH_ID_MISMATCH;
    } 
    
   printf("OTP Programmed Successfully\n"); 
   goto exit_1;
 
exit_1:
    free(data_to_program);
    data_to_program = NULL;
    write_buff[0] = SYSTEM_RESET_VALUE;
    hw_i2c_write(SYSREG_RESET_REG, write_buff, 1);
    otphal_sleep_ms(GENERAL_WAITTIME);
    return err; 
 }
