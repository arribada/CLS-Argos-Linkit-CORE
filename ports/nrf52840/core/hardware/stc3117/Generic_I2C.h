//int I2C_WriteBytes(int I2cSlaveAddr, int RegAddress, unsigned char * TxBuffer, int NumberOfBytes);
//int I2C_ReadBytes(int I2cSlaveAddr, int RegAddress, unsigned char * RxBuffer, int NumberOfBytes);

#ifndef GENERIC_I2C_H
#define GENERIC_I2C_H

// Function pointer types for I2C operations
typedef int (*I2C_WriteFunc)(int I2cSlaveAddr, int RegAddress, unsigned char *TxBuffer, int NumberOfBytes);
typedef int (*I2C_ReadFunc)(int I2cSlaveAddr, int RegAddress, unsigned char *RxBuffer, int NumberOfBytes);

// Function prototypes for registering the C++ I2C functions
void RegisterI2C_Write(I2C_WriteFunc func);
void RegisterI2C_Read(I2C_ReadFunc func);

// Add these declarations to allow calling these functions
int I2C_WriteBytes(int I2cSlaveAddr, int RegAddress, unsigned char *TxBuffer, int NumberOfBytes);
int I2C_ReadBytes(int I2cSlaveAddr, int RegAddress, unsigned char *RxBuffer, int NumberOfBytes);


#endif