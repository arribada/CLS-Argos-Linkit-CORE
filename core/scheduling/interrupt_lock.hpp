#pragma once

class InterruptLock {
public:
    InterruptLock();
    InterruptLock(const InterruptLock&) = delete;
    ~InterruptLock();
};