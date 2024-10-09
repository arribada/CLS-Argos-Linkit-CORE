#include "Generic_I2C.h"
#include <stdio.h>  // For printf
#include <stddef.h>  // For NULL definition

// Function pointer variables
static I2C_WriteFunc I2C_WriteFunction = NULL;
static I2C_ReadFunc I2C_ReadFunction = NULL;

// Register the I2C write function
void RegisterI2C_Write(I2C_WriteFunc func) {
    I2C_WriteFunction = func;
}

// Register the I2C read function
void RegisterI2C_Read(I2C_ReadFunc func) {
    I2C_ReadFunction = func;
}

// Use the registered functions to perform I2C operations
int I2C_WriteBytes(int I2cSlaveAddr, int RegAddress, unsigned char *TxBuffer, int NumberOfBytes) {
    if (I2C_WriteFunction != NULL) {
        return I2C_WriteFunction(I2cSlaveAddr, RegAddress, TxBuffer, NumberOfBytes);
    }
    return -1; // Error: Function not registered
}

int I2C_ReadBytes(int I2cSlaveAddr, int RegAddress, unsigned char *RxBuffer, int NumberOfBytes) {
    if (I2C_ReadFunction != NULL) {
        return I2C_ReadFunction(I2cSlaveAddr, RegAddress, RxBuffer, NumberOfBytes);
    }
    return -1; // Error: Function not registered
}
