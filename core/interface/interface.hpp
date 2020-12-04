#pragma once

#include <string>

class Interface
{
public:
    virtual std::string read_line() = 0;
    virtual void write(std::string str) = 0;
};