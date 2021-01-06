#pragma once

#include <cstdint>

class InterruptLock {
public:
    InterruptLock();
    InterruptLock(const InterruptLock&) = delete;
    ~InterruptLock();

private:
    uint8_t m_nested;
};