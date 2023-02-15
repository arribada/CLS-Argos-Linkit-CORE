#pragma once
#include <string>

#include "wchg.hpp"

class STWLC68 : public WirelessCharger {
public:
	void init();

	std::string get_chip_status(void) override;

	static STWLC68& get_instance()
    {
        static STWLC68 instance;
        return instance;
    }

private:
	// No copies
	STWLC68() {};
	STWLC68(STWLC68 const&)    = delete;
    void operator=(STWLC68 const&)  = delete;
};
