#include "ota_update_service.hpp"
#include "debug.hpp"

void OTAUpdateService::start(std::function<void()> const &on_connected, std::function<void()> const &on_disconnected, std::function<void()> const &on_received)
{
    DEBUG_TRACE("OTAUpdateService started");
}

void OTAUpdateService::stop()
{
    DEBUG_TRACE("OTAUpdateService stopped");
}

void OTAUpdateService::write(std::string str)
{

}

std::string OTAUpdateService::read_line()
{
    return std::string();
}
