#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "otp.h"
#include "otphal.h"

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

int get_chip_info(struct chip_info *info)
{
    int err = 0;
    uint8_t read_buff[14] = {0x00};
    
    if((err = fw_i2c_read(WLC_CHIPID_REG, read_buff, 14)) < OK) {
        printf("Error while reading device info\n");
        return err;
    }
    
    info->chip_id = (uint16_t)(read_buff[0] + (read_buff[1] << 8));
    info->chip_revision = read_buff[2];
    info->customer_id = read_buff[3];
    info->project_id = (uint16_t)(read_buff[4] + (read_buff[5] << 8));
    info->svn_revision = (uint16_t)(read_buff[6] + (read_buff[7] << 8));
    info->config_id = (uint16_t)(read_buff[8] + (read_buff[9] << 8));
    info->pe_id = (uint16_t)(read_buff[10] + (read_buff[11] << 8));
    info->patch_id = (uint16_t)(read_buff[12] + (read_buff[13] << 8));
    
    if((err = hw_i2c_read(SYSREG_HW_CUT_ID_REG, read_buff, 1)) < OK) {
        printf("Error while reading chip infon");
        return err;
    }

    info->cut_id = read_buff[0];

    printf("ChipID: %04X Chip Revision: %02X CustomerID: %02X ProjectID: %04X SVN: %04X CFG: %04X PE: %04X PAT: %04X\n", info->chip_id,
    info->chip_revision, info->customer_id, info->project_id, info->svn_revision, info->config_id, info->pe_id, info->patch_id );
    
    return OK;
}
