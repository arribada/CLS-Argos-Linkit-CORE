#ifndef __MEMORY_ACCESS_HPP_
#define __MEMORY_ACCESS_HPP_

class MemoryAccess {
public:
	virtual void *get_physical_address(unsigned int, unsigned int) = 0;
};

#endif // __MEMORY_ACCESS_HPP_
