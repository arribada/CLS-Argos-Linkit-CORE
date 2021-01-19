#include "fake_comms_scheduler.hpp"
#include "debug.hpp"

void FakeCommsScheduler::start() 
{
    DEBUG_TRACE("FakeCommsScheduler started");
}

void FakeCommsScheduler::stop() 
{
    DEBUG_TRACE("FakeCommsScheduler stopped");
}

void FakeCommsScheduler::notify_saltwater_switch_state(bool state) 
{

}

void FakeCommsScheduler::notify_sensor_log_update() 
{

}
