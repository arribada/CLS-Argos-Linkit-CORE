#include "m8q.hpp"
#include "ubx.hpp"
#include "bsp.hpp"

using namespace UBX;

#define MAX_BAUDRATE     (460800)  // The max baudrate that the M8N supports
#define DEFAULT_BAUDRATE (9600)    // The baudrate that the M8N starts on

template<typename T>
void send_packet_contents(UBX::MessageClass msgClass, uint8_t id, T contents)
{
    // Construct packet
    // Ensure header is correct
    // Ensure contents is correct
    // Calculate Checksum
    (void) contents;
}

M8QReceiver::M8QReceiver()
{
    m_nrf_uart_m8 = nullptr;
}

void M8QReceiver::power_off()
{
    delete m_nrf_uart_m8;
}

void M8QReceiver::power_on(std::function<void(GNSSData data)> data_notification_callback = nullptr)
{
    m_nrf_uart_m8 = new NrfUARTM8(UART_GPS);

    // Set the UART port to output UBX at the maximum baudrate
    CFG::PRT::MSG_UART uart_prt = 
    {
        .portID = CFG::PRT::PORTID_DDC,
        .reserved1 = 0,
        .txReady = 0,
        .mode = CFG::PRT::MODE_CHARLEN_8BIT | CFG::PRT::MODE_PARITY_NO | CFG::PRT::MODE_STOP_BITS_1,
        .baudRate = MAX_BAUDRATE,
        .inProtoMask = CFG::PRT::PROTOMASK_UBX,
        .outProtoMask = CFG::PRT::PROTOMASK_UBX,
        .flags = 0,
        .reserved2 = {0}
    };
    send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_PRT, uart_prt);

    // Enable NAV-STATUS
    CFG::MSG::MSG_MSG cfg_msg_nav_status =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_STATUS,
        .rate = 1
    };
    send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_status);

    // Enable NAV-PVT
    CFG::MSG::MSG_MSG cfg_msg_nav_pvt =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_PVT,
        .rate = 1
    };
    send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_pvt);
}


        /*UBX_SET_PACKET_HEADER(&ubx_packet, UBX_MSG_CLASS_CFG, UBX_MSG_ID_CFG_PRT, sizeof(UBX_CFG_PRT2_t));
        UBX_PAYLOAD(&ubx_packet, UBX_CFG_PRT2)->portID = 1;
        UBX_PAYLOAD(&ubx_packet, UBX_CFG_PRT2)->txReady = 0;
        UBX_PAYLOAD(&ubx_packet, UBX_CFG_PRT2)->mode = (3 << 6) | (4 << 9);
        UBX_PAYLOAD(&ubx_packet, UBX_CFG_PRT2)->inProtoMask = 1;
        UBX_PAYLOAD(&ubx_packet, UBX_CFG_PRT2)->outProtoMask = 1;
        UBX_PAYLOAD(&ubx_packet, UBX_CFG_PRT2)->flags = 0;
        UBX_PAYLOAD(&ubx_packet, UBX_CFG_PRT2)->baudRate = MAX_BAUD_RATE;*/