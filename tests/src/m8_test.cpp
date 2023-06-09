#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "fake_logger.hpp"
#include "fake_config_store.hpp"
#include "fake_rtc.hpp"
#include "fake_timer.hpp"
#include "scheduler.hpp"
#include "bsp.hpp"
#include "nrf_libuarte_async.h"
#include "m8qasync.hpp"
#include "ubx.hpp"

namespace BSP {

    const nrf_uarte_t uarte = {};
    const nrf_libuarte_async_t uarte_async = {
            .p_libuarte = &uarte,
    };
    const UARTAsync_InitTypeDefAndInst_t UARTAsync_Inits[1] =
    {
        {
            .uart = &uarte_async,
            .config = {}
        }
    };
}

class TestGNSSListener : public GPSEventListener {
private:
    GPSDevice& m_device;

    void react(const GPSEventPowerOn&) {
        mock().actualCall("GPSEventPowerOn");
    }
    void react(const GPSEventPowerOff&) {
        mock().actualCall("GPSEventPowerOff");
    }
    void react(const GPSEventError&) {
        mock().actualCall("GPSEventError");
    }
    void react(const GPSEventPVT&) {
        mock().actualCall("GPSEventPVT");
    }
    void react(const GPSEventSatReport&) {
        mock().actualCall("GPSEventSatReport");
    }
    void react(const GPSEventMaxNavSamples&) {
        mock().actualCall("GPSEventMaxNavSamples");
    }
    void react(const GPSEventMaxSatSamples&) {
        mock().actualCall("GPSEventMaxSatSamples");
    }

public:
    TestGNSSListener(GPSDevice& dev) : m_device(dev) {
        dev.subscribe(*this);
    }
    ~TestGNSSListener() {
        m_device.unsubscribe(*this);
    }
};

extern ConfigurationStore *configuration_store;
extern RTC *rtc;
extern Timer *system_timer;
extern Scheduler *system_scheduler;


TEST_GROUP(M8)
{
    FakeLog *logger;
    ConfigurationStore *fake_config;
    FakeRTC *fake_rtc;
    FakeTimer *fake_timer;
    uint64_t m_current_ms = 0;
    unsigned int m_iTOW = 0;

    void setup() {
        logger = new FakeLog("GNSS");
        fake_config = new FakeConfigurationStore;
        configuration_store = fake_config;
        fake_rtc = new FakeRTC;
        rtc = fake_rtc;
        fake_timer = new FakeTimer;
        system_timer = fake_timer;
        system_scheduler = new Scheduler(system_timer);
    }

    void teardown() {
        delete logger;
        delete fake_config;
        delete fake_timer;
        delete fake_rtc;
    }

    void increment_time_ms(uint64_t ms = 0)
    {
        while (ms)
        {
            fake_timer->increment_counter(1);
            if (fake_timer->get_counter() % 1000 == 0)
                fake_rtc->incrementtime(1);
            system_scheduler->run();
            ms--;
            //DEBUG_TRACE("timer=%lu rtc=%lu", fake_timer->get_counter(), rtc->gettime());
        }
        system_scheduler->run();
    }

    void ubx_compute_crc(const uint8_t * const buffer, const unsigned int length, uint8_t &ck_a, uint8_t &ck_b) {
        ck_a = 0;
        ck_b = 0;
        for (unsigned int i = 0; i < length; i++)
        {
            ck_a = ck_a + buffer[i];
            ck_b = ck_b + ck_a;
        }
    }

    void inject_error(unsigned int flags) {
        nrf_libuarte_async_evt_t evt;
        evt.type = NRF_LIBUARTE_ASYNC_EVT_ERROR;
        evt.data.errorsrc = flags;
        nrf_libuarte_inject_event(&evt);
    }

    template <typename T>
    void ubx_inject_message(UBX::MessageClass cls, uint8_t id, T content) {
        unsigned int content_size;
        if constexpr (std::is_same<T, UBX::Empty>::value) {
            content_size = 0;
        } else {
            content_size = sizeof(content);
        };
        UBX::HeaderAndPayloadCRC raw;
        UBX::HeaderAndPayloadCRC *msg = &raw;
        msg->syncChars[0] = UBX::SYNC_CHAR1;
        msg->syncChars[1] = UBX::SYNC_CHAR2;
        msg->msgClass = cls;
        msg->msgId = id;
        msg->msgLength = content_size;
        std::memcpy(msg->payload, &content, content_size);
        ubx_compute_crc((const uint8_t * const)&msg->msgClass, content_size + sizeof(UBX::Header) - 2,
                        msg->payload[msg->msgLength],
                        msg->payload[msg->msgLength+1]);

        nrf_libuarte_async_evt_t evt;
        evt.type = NRF_LIBUARTE_ASYNC_EVT_RX_DATA;
        evt.data.rxtx.length = content_size + sizeof(UBX::Header) + 2;
        evt.data.rxtx.p_data = (uint8_t *)&raw;
        nrf_libuarte_inject_event(&evt);
    }

    void ubx_ack(UBX::MessageClass cls, uint8_t id) {
        UBX::ACK::MSG_ACK ack = {
                cls,
                id
        };
        ubx_inject_message(UBX::MessageClass::MSG_CLASS_ACK, UBX::ACK::ID_ACK, ack);
    }

    void ubx_nack(UBX::MessageClass cls, uint8_t id) {
        UBX::ACK::MSG_NACK nack = {
                cls,
                id
        };
        ubx_inject_message(UBX::MessageClass::MSG_CLASS_ACK, UBX::ACK::ID_NACK, nack);
    }
    void ubx_pvt(double lat, double lon, bool is_valid = true) {
        UBX::NAV::PVT::MSG_PVT pvt;
        pvt.iTow = m_iTOW;
        pvt.lon = lon * 1E6;
        pvt.lat = lat * 1E6;
        pvt.valid = is_valid ? (UBX::NAV::PVT::VALID::VALID_FULLY_RESOLVED |
                UBX::NAV::PVT::VALID_VALID_DATE |
                UBX::NAV::PVT::VALID_VALID_TIME ) : 0;
        pvt.fixType = is_valid ? UBX::NAV::PVT::FIXTYPE_2D : UBX::NAV::PVT::FIXTYPE_NO;
        ubx_inject_message(UBX::MessageClass::MSG_CLASS_NAV, UBX::NAV::ID_PVT, pvt);
    }
    void ubx_status(bool is_valid) {
        UBX::NAV::STATUS::MSG_STATUS status;
        status.iTow = m_iTOW;
        status.fixStat = is_valid;
        ubx_inject_message(UBX::MessageClass::MSG_CLASS_NAV, UBX::NAV::ID_STATUS, status);
    }
    void ubx_dop() {
       UBX::NAV::DOP::MSG_DOP dop;
       dop.iTow = m_iTOW;
       ubx_inject_message(UBX::MessageClass::MSG_CLASS_NAV, UBX::NAV::ID_DOP, dop);
    }
    void ubx_mga_ack(bool success, unsigned int num_messages) {
        UBX::MGA::MSG_ACK ack;
        ack.infoCode = !success;
        ack.msgPayloadStart = num_messages;
        ubx_inject_message(UBX::MessageClass::MSG_CLASS_MGA, UBX::MGA::ID_ACK, ack);
    }
    void ubx_mga_dbd(unsigned int num_messages) {
        struct {
            uint8_t data[64];
        } x;
        for (unsigned int i = 0; i < num_messages; i++) {
            ubx_inject_message(UBX::MessageClass::MSG_CLASS_MGA, UBX::MGA::ID_DBD, x);
            increment_time_ms();
        }
    }
};

TEST(M8, FailedToSyncCommsError)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = false;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    mock().expectOneCall("GPSEventError");
    increment_time_ms(6000);
    m.power_off();
    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms();
}

TEST(M8, FailedToChangeBaudRate)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = false;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    mock().expectOneCall("GPSEventError");
    increment_time_ms(3000);
    m.power_off();
    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms();
}

TEST(M8, FailedToReceivePVT)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = false;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    increment_time_ms(200);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_CFG);
    increment_time_ms();
    // !!! Soft Reset !!!
    increment_time_ms(1000);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_ODO);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PM2);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_RXM);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAV5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAVX5);
    increment_time_ms();

    // Start receive
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();

    // Receive
    mock().expectOneCall("GPSEventError");
    increment_time_ms(5000);
    m.power_off();
    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms();
}

TEST(M8, PVTReportAfterPowerOffDemand)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = false;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    increment_time_ms(200);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_CFG);
    increment_time_ms();
    // !!! Soft Reset !!!
    increment_time_ms(1000);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_ODO);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PM2);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_RXM);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAV5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAVX5);
    increment_time_ms();

    // Start receive
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();

    // Receive
    m.power_off();
    ubx_pvt(-12, 20);
    ubx_status(true);
    ubx_dop();
    increment_time_ms();

    // Stop receiving
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();

    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms(100);
}

TEST(M8, PVTReportSuccessWithoutANO)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = false;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    increment_time_ms(200);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_CFG);
    increment_time_ms();
    // !!! Soft Reset !!!
    increment_time_ms(1000);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_ODO);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PM2);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_RXM);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAV5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAVX5);
    increment_time_ms();

    // Start receive
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();

    // Receive
    mock().expectOneCall("GPSEventPVT");
    ubx_pvt(-12, 20);
    ubx_status(true);
    ubx_dop();
    increment_time_ms();

    // Stop receiving
    m.power_off();
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();

    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms(100);
}

TEST(M8, PVTReportSuccessWithANO)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = true;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    increment_time_ms(200);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_CFG);
    increment_time_ms();
    // !!! Soft Reset !!!
    increment_time_ms(1000);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_ODO);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PM2);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_RXM);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAV5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAVX5);
    increment_time_ms();

    // Start receive
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();

    // Receive
    mock().expectOneCall("GPSEventPVT");
    ubx_pvt(-12, 20);
    ubx_status(true);
    ubx_dop();
    increment_time_ms();

    // Stop receiving
    m.power_off();
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    increment_time_ms(100);

    ubx_mga_ack(true, 56);
    ubx_mga_dbd(56);
    increment_time_ms();
    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms(1000);
}

TEST(M8, PVTReportSuccessWithANOMissingDBDAck)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = true;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    increment_time_ms(200);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_CFG);
    increment_time_ms();
    // !!! Soft Reset !!!
    increment_time_ms(1000);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_ODO);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PM2);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_RXM);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAV5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAVX5);
    increment_time_ms();

    // Start receive
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();

    // Receive
    mock().expectOneCall("GPSEventPVT");
    ubx_pvt(-12, 20);
    ubx_status(true);
    ubx_dop();
    increment_time_ms();

    // Stop receiving
    m.power_off();
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    increment_time_ms(100);

    increment_time_ms();
    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms(1000);
}

TEST(M8, PVTReportSuccessWithANOMissingDBDs)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = true;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    increment_time_ms(200);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_CFG);
    increment_time_ms();
    // !!! Soft Reset !!!
    increment_time_ms(1000);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_ODO);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PM2);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_RXM);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAV5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAVX5);
    increment_time_ms();

    // Start receive
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();

    // Receive
    mock().expectOneCall("GPSEventPVT");
    ubx_pvt(-12, 20);
    ubx_status(true);
    ubx_dop();
    increment_time_ms();

    // Stop receiving
    m.power_off();
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    increment_time_ms(100);

    // Send MGA-DBD
    ubx_mga_ack(true, 56);
    increment_time_ms(1000);
    ubx_mga_ack(true, 56);
    increment_time_ms(1000);
    ubx_mga_ack(true, 56);
    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms(1000);
}


TEST(M8, PVTReportSuccessWithANOAndStartNewReceive)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = true;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    increment_time_ms(200);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_CFG);
    increment_time_ms();
    // !!! Soft Reset !!!
    increment_time_ms(1000);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_ODO);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PM2);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_RXM);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAV5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAVX5);
    increment_time_ms();

    // Start receive
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();

    // Receive
    mock().expectOneCall("GPSEventPVT");
    ubx_pvt(-12, 20);
    ubx_status(true);
    ubx_dop();
    increment_time_ms();

    // Stop receiving
    m.power_off();
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    increment_time_ms(100);

    // Send MGA-DBD
    ubx_mga_ack(true, 56);
    increment_time_ms();
    ubx_mga_dbd(56);
    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms(1000);


    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    increment_time_ms(200);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_CFG);
    increment_time_ms();
    // !!! Soft Reset !!!
    increment_time_ms(1000);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_ODO);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PM2);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_RXM);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAV5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAVX5);
    increment_time_ms();
    ubx_mga_ack(true, 0);
    increment_time_ms();

    for (unsigned int i = 0; i < 31; i++) {
        increment_time_ms(5);
    }
    for (unsigned int i = 0; i < 56; i++) {
        ubx_mga_ack(true, 0);
        increment_time_ms();
    }
    increment_time_ms(5);

    // Start receive
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
}


TEST(M8, FailedToStartReceive)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = false;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    increment_time_ms(1000);
    increment_time_ms(1000);
    increment_time_ms(1000);
    mock().expectOneCall("GPSEventError");
    increment_time_ms(1000);
    m.power_off();
    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms();
}


TEST(M8, UartCommsErrorDuringReceive)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = false;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    increment_time_ms(200);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_CFG);
    increment_time_ms();
    // !!! Soft Reset !!!
    increment_time_ms(1000);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_ODO);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PM2);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_RXM);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAV5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAVX5);
    increment_time_ms();

    // Start receive
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();

    // Receive
    inject_error(0x01);
    increment_time_ms();
    inject_error(0x01);
    increment_time_ms();
    inject_error(0x01);
    mock().expectOneCall("GPSEventError");
    increment_time_ms();
    m.power_off();
    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms();
}


TEST(M8, UartCommsErrorDuringConfig)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = false;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    increment_time_ms();
    inject_error(0x01);
    increment_time_ms();
    inject_error(0x01);
    increment_time_ms();
    inject_error(0x01);
    mock().expectOneCall("GPSEventError");
    increment_time_ms();
    m.power_off();
    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms();
}

TEST(M8, UartCommsErrorDuringConfigAndRecover)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = false;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    inject_error(0x01);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    inject_error(0x01);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
}


TEST(M8, PVTReportSuccessWithMGAOverflow)
{
    M8QAsyncReceiver m;
    TestGNSSListener listener(m);
    GPSNavSettings settings;
    settings.assistnow_autonomous_enable = true;
    settings.assistnow_offline_enable = false;
    settings.debug_enable = true;
    settings.dyn_model = BaseGNSSDynModel::PORTABLE;
    settings.fix_mode = BaseGNSSFixMode::AUTO;
    settings.hacc_filter_en = false;
    settings.hdop_filter_en = false;
    settings.max_nav_samples = 30;

    // Power on
    mock().expectOneCall("set").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    mock().expectOneCall("delay_ms").ignoreOtherParameters();
    mock().expectOneCall("GPSEventPowerOn");
    m.power_on(settings);
    increment_time_ms();
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    // Configure
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PRT);
    increment_time_ms(500);
    ubx_nack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_GNSS);
    increment_time_ms();
    increment_time_ms(200);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_CFG);
    increment_time_ms();
    // !!! Soft Reset !!!
    increment_time_ms(1000);
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_ODO);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_TP5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_PM2);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_RXM);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAV5);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_NAVX5);
    increment_time_ms();

    // Start receive
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();

    // Receive
    mock().expectOneCall("GPSEventPVT");
    ubx_pvt(-12, 20);
    ubx_status(true);
    ubx_dop();
    increment_time_ms();

    m.power_off();
    increment_time_ms();

    // Stop receiving
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    ubx_ack(UBX::MessageClass::MSG_CLASS_CFG, UBX::CFG::ID_MSG);
    increment_time_ms();
    increment_time_ms(100);

    // Fetch database
    ubx_mga_ack(true, 300);
    increment_time_ms();
    ubx_mga_dbd(300);
    mock().expectOneCall("GPSEventPowerOff");
    mock().expectOneCall("clear").withParameter("pin", BSP::GPIO::GPIO_GPS_PWR_EN);
    increment_time_ms(1000);
}
