#pragma once

#include "error.hpp"
#include "memory_access.hpp"

#define RAM_START_ADDR (0x20000000)
#define RAM_END_ADDR   (0x20040000)

class NrfMemoryAccess : public MemoryAccess {
public:
	void *get_physical_address(unsigned int addr, unsigned int length) override {
		if (addr + length >= RAM_END_ADDR)
			throw ILLEGAL_MEMORY_ADDRESS;
		if (addr < 0x20000000)
			throw ILLEGAL_MEMORY_ADDRESS;
		
		return reinterpret_cast<void *>(addr);
	}
};