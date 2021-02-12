#include "m8q.hpp"
#include "bsp.hpp"
#include "gpio.hpp"
#include "nrf_delay.h"

using namespace UBX;

template<typename T>
bool M8QReceiver::send_packet_contents(UBX::MessageClass msgClass, uint8_t id, T contents, bool expect_ack)
{
    constexpr uint32_t ack_timeout_ms = 1000;
    constexpr uint32_t ack_timeout_steps = 100;

    // Construct packet header
    UBX::Header header = {
        .syncChars = {UBX::SYNC_CHAR1, UBX::SYNC_CHAR2},
        .msgClass = msgClass,
        .msgId = id,
        .msgLength = sizeof(contents)
    };

    // Calculate the checksum
    uint8_t ck[2] = {0};
    uint8_t *checksum_ptr = reinterpret_cast<uint8_t *>(&header.msgClass);

    // Calculate the checksum across the msgClass, msgId and msgLength
    for (unsigned int i = 0; i < 4; i++)
    {
        ck[0] = ck[0] + checksum_ptr[i];
        ck[1] = ck[1] + ck[0];
    }

    // Calculate the checksum across the contents
    checksum_ptr = reinterpret_cast<uint8_t *>(&contents);
    for (unsigned int i = 0; i < sizeof(contents); i++)
    {
        ck[0] = ck[0] + checksum_ptr[i];
        ck[1] = ck[1] + ck[0];
    }

    m_nrf_uart_m8->send(reinterpret_cast<uint8_t *>(&header), sizeof(header));
    m_nrf_uart_m8->send(reinterpret_cast<uint8_t *>(&contents), sizeof(contents));
    m_nrf_uart_m8->send(&ck[0], sizeof(ck));

    // Wait for an ACK/NACK if we are expecting one
    if (expect_ack)
    {
        for (uint32_t i = 0; i < ack_timeout_steps; ++i)
        {
            if (m_rx_buffer.pending)
            {
                UBX::Header *header_ptr = reinterpret_cast<UBX::Header *>(&m_rx_buffer.data[0]);
                if (header_ptr->msgClass == UBX::MessageClass::MSG_CLASS_ACK)
                {
                    if (header_ptr->msgId == UBX::ACK::ID_ACK)
                    {
                        UBX::ACK::MSG_ACK *msg_ack_ptr = reinterpret_cast<UBX::ACK::MSG_ACK *>(&m_rx_buffer.data[sizeof(UBX::Header)]);
                        if (msg_ack_ptr->clsID == msgClass && msg_ack_ptr->msgID == id)
                        {
                            m_rx_buffer.pending = false;
                            DEBUG_TRACE("GPS ACK-ACK");
                            return true; // Message was ACKed
                        }
                    }
                    else if (header_ptr->msgId == UBX::ACK::ID_NACK)
                    {
                        UBX::ACK::MSG_ACK *msg_nack_ptr = reinterpret_cast<UBX::ACK::MSG_ACK *>(&m_rx_buffer.data[sizeof(UBX::Header)]);
                        if (msg_nack_ptr->clsID == msgClass && msg_nack_ptr->msgID == id)
                        {
                            m_rx_buffer.pending = false;
                            DEBUG_WARN("GPS ACK-NACK");
                            return false; // Message was NACKed
                        }
                    }
                }

                m_rx_buffer.pending = false;
            }

            nrf_delay_ms(ack_timeout_ms / ack_timeout_steps);
        }

        DEBUG_ERROR("GPS Timed out waiting for response");
        return false;
    }

    return true;
}

M8QReceiver::M8QReceiver()
{
    m_nrf_uart_m8 = nullptr;
    m_rx_buffer.pending = false;
    m_has_booted = false;
}

void M8QReceiver::power_off()
{
    delete m_nrf_uart_m8;

    // Disable the power supply for the GPS
    GPIOPins::clear(BSP::GPIO::GPIO_GPS_PWR_EN);

    m_has_booted = false;
    m_rx_buffer.pending = false;
}

void M8QReceiver::power_on(std::function<void(GNSSData data)> data_notification_callback = nullptr)
{
    m_data_notification_callback = data_notification_callback;
    
    m_nrf_uart_m8 = new NrfUARTM8(UART_GPS, [this](uint8_t *data, size_t len){ reception_callback(data, len); });

    // Clear our dop and pvt message containers
    memset(&m_last_received_pvt, 0, sizeof(m_last_received_pvt));
    memset(&m_last_received_dop, 0, sizeof(m_last_received_dop));

    // Enable the power supply for the GPS
    GPIOPins::set(BSP::GPIO::GPIO_GPS_PWR_EN);

    nrf_delay_ms(1000); // Necessary to allow the device to boot
    
    setup_uart_port();
    setup_gnss_channel_sharing();
    disable_odometer();
    disable_timepulse_output();
    setup_power_management();
    setup_lower_power_mode();
    setup_simple_navigation_settings();
    setup_expert_navigation_settings();
    enable_nav_pvt_message();
    enable_nav_dop_message();

    m_has_booted = true;
}

void M8QReceiver::reception_callback(uint8_t *data, size_t len)
{
    // If we haven't booted then we need to stash the incoming messages so that we can check for ACK/NACKs whilst configuring the device
    if (!m_has_booted)
    {
        if (!m_rx_buffer.pending)
        {
            memcpy(&m_rx_buffer.data[0], data, len);
            m_rx_buffer.pending = true;
            m_rx_buffer.len = len;
        }

        m_rx_buffer.pending = true;
    }
    else
    {
        // We're looking for a pair of NAV-PVT and NAV-DOP messages so we can retrieve all the data necessary to genereate a callback
        // We can use iTow to ensure both messages relate to the same position/time
        UBX::Header *header_ptr = reinterpret_cast<UBX::Header *>(&data[0]);
        if (header_ptr->msgClass == UBX::MessageClass::MSG_CLASS_NAV)
        {
            if (header_ptr->msgId == UBX::NAV::ID_PVT)
            {
                UBX::NAV::PVT::MSG_PVT *msg_pvt_ptr = reinterpret_cast<UBX::NAV::PVT::MSG_PVT *>(&data[sizeof(UBX::Header)]);
                memcpy(&m_last_received_pvt, msg_pvt_ptr, sizeof(m_last_received_pvt));

                DEBUG_TRACE("GPS NAV-PVT");

                populate_gnss_data_and_callback();
            }
            else if (header_ptr->msgId == UBX::NAV::ID_DOP)
            {
                UBX::NAV::DOP::MSG_DOP *msg_dop_ptr = reinterpret_cast<UBX::NAV::DOP::MSG_DOP *>(&data[sizeof(UBX::Header)]);
                memcpy(&m_last_received_dop, msg_dop_ptr, sizeof(m_last_received_dop));

                DEBUG_TRACE("GPS NAV-DOP");

                populate_gnss_data_and_callback();
            }
        }
    }
}

// Checks if the data we have to date is enough to generate a gnss data callback and if it is calls it
void M8QReceiver::populate_gnss_data_and_callback()
{
    if (m_last_received_pvt.fixType != UBX::NAV::PVT::FIXTYPE_NO && // GPS Fix must be achieved
        m_last_received_pvt.valid & UBX::NAV::PVT::VALID_VALID_DATE && // Date must be valid
        m_last_received_pvt.valid & UBX::NAV::PVT::VALID_VALID_TIME && // Time must be valid
        m_last_received_pvt.iTow == m_last_received_dop.iTow) // Both received DOP and NAV must refer to the same position
    {
        GNSSData gnss_data = 
        {
            .day       = m_last_received_pvt.day,
            .month     = m_last_received_pvt.month,
            .year      = m_last_received_pvt.year,
            .hours     = m_last_received_pvt.hour,
            .minutes   = m_last_received_pvt.min,
            .seconds   = m_last_received_pvt.sec,
            .latitude  = m_last_received_pvt.lat / 10000000.0,
            .longitude = m_last_received_pvt.lon / 10000000.0,
            .hDOP      = m_last_received_dop.hDOP / 100.0
        };

        DEBUG_TRACE("GPS Pos - %02u/%02u/%04u %02u:%02u:%02u lat: %f lon: %f hDOP: %f",
                    gnss_data.day, gnss_data.month, gnss_data.year,
                    gnss_data.hours, gnss_data.minutes, gnss_data.seconds,
                    gnss_data.latitude, gnss_data.longitude, gnss_data.hDOP);

        if (m_data_notification_callback)
            m_data_notification_callback(gnss_data);
    }
}

bool M8QReceiver::setup_uart_port()
{
    // Disable NMEA and only allow UBX messages

    CFG::PRT::MSG_UART uart_prt = 
    {
        .portID = CFG::PRT::PORTID_UART,
        .reserved1 = 0,
        .txReady = 0,
        .mode = CFG::PRT::MODE_CHARLEN_8BIT | CFG::PRT::MODE_PARITY_NO | CFG::PRT::MODE_STOP_BITS_1,
        .baudRate = 9600,
        .inProtoMask = CFG::PRT::PROTOMASK_UBX,
        .outProtoMask = CFG::PRT::PROTOMASK_UBX,
        .flags = 0,
        .reserved2 = {0}
    };

    DEBUG_TRACE("GPS CFG-PRT");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_PRT, uart_prt);
}

bool M8QReceiver::setup_gnss_channel_sharing()
{
    CFG::GNSS::MSG_GNSS cfg_msg_cfg_gnss =
    {
        .version = 0x00,
        .numTrkChHw = 0,
        .numTrkChUse = 0xFF, // Use max number of tracking channels
        .numConfigBlocks = 7,
        .data =
        {
            {
                .gnssId = CFG::GNSS::GNSSID_GPS,
                .resTrkCh = 8,
                .maxTrkCh = 16,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_GPS_L1CA | CFG::GNSS::FLAGS_ENABLE
            },
            {
                .gnssId = CFG::GNSS::GNSSID_SBAS,
                .resTrkCh = 1,
                .maxTrkCh = 3,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_SBAS_L1CA
            },
            {
                .gnssId = CFG::GNSS::GNSSID_GALILEO,
                .resTrkCh = 4,
                .maxTrkCh = 8,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_GALILEO_E1 | CFG::GNSS::FLAGS_ENABLE
            },
            {
                .gnssId = CFG::GNSS::GNSSID_BEIDOU,
                .resTrkCh = 8,
                .maxTrkCh = 16,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_BEIDOU_B1I
            },
            {
                .gnssId = CFG::GNSS::GNSSID_IMES,
                .resTrkCh = 0,
                .maxTrkCh = 8,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_IMES_L1
            },
            {
                .gnssId = CFG::GNSS::GNSSID_QZSS,
                .resTrkCh = 0,
                .maxTrkCh = 3,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_QZSS_L1CA
            },
            {
                .gnssId = CFG::GNSS::GNSSID_GLONASS,
                .resTrkCh = 8,
                .maxTrkCh = 14,
                .reserved1 = 0,
                .flags = CFG::GNSS::FLAGS_GLONASS_L1 | CFG::GNSS::FLAGS_ENABLE
            }
        }
    };

    DEBUG_TRACE("GPS CFG-GNSS");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_GNSS, cfg_msg_cfg_gnss);
}

bool M8QReceiver::setup_power_management()
{
    CFG::PM2::MSG_PM2 cfg_msg_cfg_pm2 =
    {
        .version = 0x02,
        .reserved1 = 0,
        .maxStartupStateDur = 0,
        .reserved2 = 0,
        .flags = CFG::PM2::FLAGS_DONOTENTEROFF | CFG::PM2::FLAGS_UPDATEEPH | CFG::PM2::FLAGS_MODE_CYCLIC | CFG::PM2::FLAGS_LIMITPEAKCURR_DISABLED,
        .updatePeriod = 1000,
        .searchPeriod = 10000,
        .gridOffset = 0,
        .onTime = 0,
        .minAcqTime = 300,
        .reserved3 = {0},
        .extintInactivityMs = 0
    };

    DEBUG_TRACE("GPS CFG-PM2");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_PM2, cfg_msg_cfg_pm2);
}

bool M8QReceiver::setup_lower_power_mode()
{
    CFG::RXM::MSG_RXM cfg_msg_cfg_rxm =
    {
        .reserved1 = 0,
        .lpMode = CFG::RXM::CONTINUOUS_MODE
    };

    DEBUG_TRACE("GPS CFG-RXM");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_RXM, cfg_msg_cfg_rxm);
}

bool M8QReceiver::setup_simple_navigation_settings()
{
    CFG::NAV5::MSG_NAV5 cfg_msg_cfg_nav5 =
    {
        .mask = CFG::NAV5::MASK_DYN | CFG::NAV5::MASK_MIN_EL | CFG::NAV5::MASK_POS_FIX_MODE | CFG::NAV5::MASK_POS_MASK | CFG::NAV5::MASK_TIME_MASK | CFG::NAV5::MASK_STATIC_HOLD_MASK | CFG::NAV5::MASK_DGPS_MASK | CFG::NAV5::MASK_CNO_THRESHOLD | CFG::NAV5::MASK_UTC,
        .dynModel = CFG::NAV5::DYNMODEL_SEA,
        .fixMode = CFG::NAV5::FIXMODE_2D_ONLY,
        .fixedAlt = 0,
        .fixedAltVar = 10000,
        .minElev = 5,
        .drLimit = 0,
        .pDop = 250,
        .tDop = 250,
        .pAcc = 100,
        .tAcc = 350,
        .staticHoldThresh = 0,
        .dgnssTimeOut = 60,
        .cnoThreshNumSVs = 0,
        .cnoThresh = 0,
        .reserved1 = {0},
        .staticHoldMaxDist = 0,
        .utcStandard = 0,
        .reserved2 = {0}
    };

    DEBUG_TRACE("GPS CFG-NAV5");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_NAV5, cfg_msg_cfg_nav5);
}

bool M8QReceiver::setup_expert_navigation_settings()
{
    CFG::NAVX5::MSG_NAVX5 cfg_msg_cfg_navx5 =
    {
        .version = 0x0002,
        .mask1 = CFG::NAVX5::MASK1_MIN_MAX | CFG::NAVX5::MASK1_MIN_CNO | CFG::NAVX5::MASK1_INITIAL_3D_FIX | CFG::NAVX5::MASK1_WKN_ROLL | CFG::NAVX5::MASK1_ACK_AID | CFG::NAVX5::MASK1_PPP | CFG::NAVX5::MASK1_AOP,
        .mask2 = 0,
        .reserved1 = {0},
        .minSVs = 3,
        .maxSVs = 32,
        .minCNO = 6,
        .reserved2 = 0,
        .iniFix3D = 0,
        .reserved3 = {0},
        .ackAiding = 1,
        .wknRollover = 0, // Use firmware default
        .sigAttenCompMode = 0,
        .reserved4 = 0,
        .reserved5 = {0},
        .reserved6 = {0},
        .usePPP = 0,
        .aopCfg = 0, // AssistNow Autonomous disabled //CFG::NAVX5::AOPCFG_USE_AOP,
        .reserved7 = {0},
        .aopOrbMaxErr = 100,
        .reserved8 = {0},
        .reserved9 = {0},
        .useAdr = 0
    };

    DEBUG_TRACE("GPS CFG-NAVX5");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_NAVX5, cfg_msg_cfg_navx5);
}

bool M8QReceiver::disable_odometer()
{
    CFG::ODO::MSG_ODO cfg_msg_cfg_odo =
    {
        .version = 0x00,
        .reserved1 = {0},
        .flags = 0,
        .odoCfg = 0,
        .reserved2 = {0},
        .cogMaxSpeed = 1,
        .cogMaxPosAcc = 50,
        .reserved3 = {0},
        .velLpGain = 153,
        .cogLpGain = 76,
        .reserved4 = {0}
    };

    DEBUG_TRACE("GPS CFG-ODO");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_ODO, cfg_msg_cfg_odo);
}

bool M8QReceiver::disable_timepulse_output()
{
    // Disable timepulse 1
    CFG::TP5::MSG_TP5 cfg_msg_cfg_tp5 =
    {
        .tpIdx = 0,
        .version = 0x01,
        .reserved1 = {0},
        .antCableDelay = 0,
        .rfGroupDelay = 0,
        .freqPeriod = 1000000,
        .freqPeriodLock = 1000000,
        .pulseLenRatio = 0,
        .pulseLenRatioLock = 100000,
        .userConfigDelay = 0,
        .flags = 0
    };

    DEBUG_TRACE("GPS CFG-TP5");

    if (!send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_TP5, cfg_msg_cfg_tp5))
        return false;

    // Disable timepulse 2

    cfg_msg_cfg_tp5.tpIdx = 1;

    DEBUG_TRACE("GPS CFG-TP5");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_TP5, cfg_msg_cfg_tp5);
}

bool M8QReceiver::enable_nav_pvt_message()
{
    CFG::MSG::MSG_MSG cfg_msg_nav_pvt =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_PVT,
        .rate = 1
    };

    DEBUG_TRACE("GPS CFG-MSG");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_pvt);
}

bool M8QReceiver::enable_nav_dop_message()
{
    CFG::MSG::MSG_MSG cfg_msg_nav_dop =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_DOP,
        .rate = 1
    };

    DEBUG_TRACE("GPS CFG-MSG");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_dop);
}
