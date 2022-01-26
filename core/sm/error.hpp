#ifndef __ERROR_HPP_
#define __ERROR_HPP_

enum ErrorCode : int {
	BAD_FILESYSTEM,
	DTE_PROTOCOL_VALUE_OUT_OF_RANGE,
	DTE_PROTOCOL_MESSAGE_TOO_LARGE,
	DTE_PROTOCOL_UNKNOWN_COMMAND,
	DTE_PROTOCOL_BAD_FORMAT,
	DTE_PROTOCOL_PAYLOAD_LENGTH_MISMATCH,
	DTE_PROTOCOL_MISSING_ARG,
	DTE_PROTOCOL_UNEXPECTED_ARG,
	DTE_PROTOCOL_PARAM_KEY_UNRECOGNISED,
	CONFIG_DOES_NOT_EXIST,
	CONFIG_STORE_CORRUPTED,
	ILLEGAL_MEMORY_ADDRESS,
	SPI_COMMS_ERROR,
	ARTIC_CRC_FAILURE,
	ARTIC_IRQ_TIMEOUT,
	ARTIC_INCORRECT_STATUS,
	ARTIC_BOOT_TIMEOUT,
	RESOURCE_NOT_AVAILABLE,
	OTA_TRANSFER_NOT_STARTED,
	OTA_TRANSFER_INCOMPLETE,
	OTA_TRANSFER_ALREADY_IN_PROGRESS,
	OTA_TRANSFER_CRC_ERROR,
	OTA_TRANSFER_OVERFLOW,
	OTA_TRANSFER_BAD_FILE_SIZE,
	OTA_TRANSFER_INVALID_FILE_ID,
	OTA_TRANSFER_FLASH_ERROR,
	GPS_BOOT_TIMEOUT,
	GPS_BOOT_NACKED,
	NOT_IMPLEMENTED,
	I2C_COMMS_ERROR
};

#endif // __ERROR_HPP_
