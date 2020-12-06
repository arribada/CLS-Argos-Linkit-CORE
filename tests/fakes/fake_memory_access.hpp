#ifndef __FAKE_MEMORY_HPP_
#define __FAKE_MEMORY_HPP_

#include "error.hpp"
#include "memory_access.hpp"


class FakeMemoryAccess : public MemoryAccess {
private:
	static inline unsigned char ram[1024*1024];

public:
	FakeMemoryAccess() {
		for (unsigned int i = 0; i < sizeof(ram); i++)
			ram[i] = (unsigned char)i;
	}
	void *get_physical_address(unsigned int addr, unsigned int length) override {
		if (addr + length >= sizeof(ram))
			throw ILLEGAL_MEMORY_ADDRESS;
		return (void *)&ram[addr];
	}
};

#endif // __FAKE_MEMORY_HPP_
