#include "m8q.hpp"
#include "bsp.hpp"
#include "gpio.hpp"
#include "nrf_delay.h"
#include "rtc.hpp"
#include "error.hpp"
#include "debug.hpp"
#include "timeutils.hpp"
#include "ubx_ano.hpp"

extern RTC *rtc;
extern FileSystem *main_filesystem;

using namespace UBX;

template <typename T>
M8QReceiver::SendReturnCode M8QReceiver::send_packet_contents(UBX::MessageClass msgClass, uint8_t id, T contents, bool expect_ack)
{
    const std::time_t timeout = 2;

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

    m_rx_buffer.pending = false;
    m_nrf_uart_m8->send(reinterpret_cast<uint8_t *>(&header), sizeof(header));
    m_nrf_uart_m8->send(reinterpret_cast<uint8_t *>(&contents), sizeof(contents));
    m_nrf_uart_m8->send(&ck[0], sizeof(ck));

    // Wait for an ACK/NACK if we are expecting one
    if (expect_ack)
    {
        auto start_time = rtc->gettime();
        for (;;)
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
                            DEBUG_TRACE("GPS ACK-ACK <-");
                            return SendReturnCode::SUCCESS;
                        }
                    }
                    else if (header_ptr->msgId == UBX::ACK::ID_NACK)
                    {
                        UBX::ACK::MSG_ACK *msg_nack_ptr = reinterpret_cast<UBX::ACK::MSG_ACK *>(&m_rx_buffer.data[sizeof(UBX::Header)]);
                        if (msg_nack_ptr->clsID == msgClass && msg_nack_ptr->msgID == id)
                        {
                            DEBUG_TRACE("GPS ACK-NACK <-");
                            return SendReturnCode::NACKD;
                        }
                    }
                }

                m_rx_buffer.pending = false;
            }

            if (rtc->gettime() - start_time >= timeout)
            {
                DEBUG_ERROR("GPS Timed out waiting for response");
                return SendReturnCode::RESPONSE_TIMEOUT;
            }
        }
    }

    return SendReturnCode::SUCCESS;
}

template <typename T>
M8QReceiver::SendReturnCode M8QReceiver::poll_contents_and_collect(UBX::MessageClass msgClass, uint8_t id, T &contents)
{
    const std::time_t timeout = 3;

    // Construct packet header
    UBX::Header header = {
        .syncChars = {UBX::SYNC_CHAR1, UBX::SYNC_CHAR2},
        .msgClass = msgClass,
        .msgId = id,
        .msgLength = 0
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

    m_rx_buffer.pending = false;
    m_nrf_uart_m8->send(reinterpret_cast<uint8_t *>(&header), sizeof(header));
    m_nrf_uart_m8->send(&ck[0], sizeof(ck));

    // Wait for the poll response
    auto start_time = rtc->gettime();
    for (;;)
    {
        if (m_rx_buffer.pending)
        {
            UBX::Header *header_ptr = reinterpret_cast<UBX::Header *>(&m_rx_buffer.data[0]);
            if (header_ptr->msgClass == msgClass && header_ptr->msgId == id)
            {
                if (m_rx_buffer.len == sizeof(T) + sizeof(UBX::Header) + sizeof(ck))
                {
                    memcpy(&contents, &m_rx_buffer.data[sizeof(UBX::Header)], sizeof(T));
                    return SendReturnCode::SUCCESS;
                }
            }

            m_rx_buffer.pending = false;
        }

        if (rtc->gettime() - start_time >= timeout)
        {
            DEBUG_ERROR("GPS Timed out waiting for response");
            return SendReturnCode::RESPONSE_TIMEOUT;
        }
    }

    return SendReturnCode::SUCCESS;
}

M8QReceiver::SendReturnCode M8QReceiver::poll_contents(UBX::MessageClass msgClass, uint8_t id)
{
    // Construct packet header
    UBX::Header header = {
        .syncChars = {UBX::SYNC_CHAR1, UBX::SYNC_CHAR2},
        .msgClass = msgClass,
        .msgId = id,
        .msgLength = 0
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

    m_nrf_uart_m8->send(reinterpret_cast<uint8_t *>(&header), sizeof(header));
    m_nrf_uart_m8->send(&ck[0], sizeof(ck));

    return SendReturnCode::SUCCESS;
}

M8QReceiver::M8QReceiver()
{
    m_nrf_uart_m8 = nullptr;
    m_navigation_database_len = 0;
    m_rx_buffer.pending = false;
    m_capture_messages = false;
    m_assistnow_autonomous_enable = false;
    m_assistnow_offline_enable = false;
    m_state = State::POWERED_OFF;
}

M8QReceiver::~M8QReceiver()
{
	power_off();
}

void M8QReceiver::power_off()
{

#if 0 == NO_GPS_POWER_REG
	// We can rely upon the regulator state having powered off the device
	if (State::POWERED_OFF == m_state)
		return;
#endif

	// In case of no power regulator, forcibly run through a shutdown sequence e.g.,
	// to cover the scenario when the board is initially powered for the first time
	exit_shutdown_mode();

	// This code should be callable even if the device is powered off already
    m_state = State::POWERED_OFF;

    DEBUG_INFO("M8QReceiver::power_off");

    m_capture_messages = true;

    // Disable any messages so they don't interrupt our navigation database dump
    if (SendReturnCode::SUCCESS == setup_uart_port()) {
		disable_nav_pvt_message();
		disable_nav_status_message();
		disable_nav_dop_message();
		if (m_assistnow_autonomous_enable)
			fetch_navigation_database();
		setup_power_management();
    } else {
    	DEBUG_ERROR("M8QReceiver: power_off: failed to configure UART");
    }

    m_capture_messages = false;

    enter_shutdown_mode();

    m_rx_buffer.pending = false;
    m_data_notification_callback = nullptr;
}

M8QReceiver::SendReturnCode M8QReceiver::print_version()
{
    // Request the receiver and software version
    poll_contents(UBX::MessageClass::MSG_CLASS_MON, UBX::MON::ID_VER);

    auto start_time = rtc->gettime();
    const std::time_t timeout = 3;
    
    for (;;)
    {
        if (m_rx_buffer.pending)
        {
            UBX::Header *header_ptr = reinterpret_cast<UBX::Header *>(&m_rx_buffer.data[0]);
            if (header_ptr->msgClass == UBX::MessageClass::MSG_CLASS_MON && header_ptr->msgId == UBX::MON::ID_VER)
            {
                uint8_t *data_ptr = &m_rx_buffer.data[sizeof(UBX::Header)];
                DEBUG_TRACE("M8QReceiver::print_version");
                DEBUG_TRACE("%.*s", 30, &data_ptr[0]);  // swVersion
                DEBUG_TRACE("%.*s", 10, &data_ptr[30]); // hwVersion
                uint32_t number_of_extensions = (header_ptr->msgLength - 40) / 30;
                for (uint32_t i = 0; i < number_of_extensions; ++i)
                {
                    DEBUG_TRACE("%.*s", 30, &data_ptr[40 + (i * 30)]); // Extension
                }
                return SendReturnCode::SUCCESS;
            }

            m_rx_buffer.pending = false;
        }

        if (rtc->gettime() - start_time >= timeout)
        {
            DEBUG_ERROR("M8QReceiver::print_version: timed out waiting for response");
            m_navigation_database_len = 0; // Invalidate the data we did receive
            return SendReturnCode::RESPONSE_TIMEOUT;
        }
    }

    return SendReturnCode::SUCCESS;
}

M8QReceiver::SendReturnCode M8QReceiver::fetch_navigation_database()
{
    const std::time_t timeout = 3;
    uint8_t *write_dst = &m_navigation_database[0];
    m_navigation_database_len = 0;
    uint32_t number_of_mga_dbd_msgs = 0;

    m_capture_messages = true;

    DEBUG_TRACE("M8QReceiver::fetch_navigation_database: requesting navigation database");

    // Request a dump of the navigation database
    m_rx_buffer.pending = false;
    poll_contents(UBX::MessageClass::MSG_CLASS_MGA, UBX::MGA::ID_DBD);

    auto last_update = rtc->gettime();
    
    for (;;)
    {
        if (m_rx_buffer.pending)
        {
            UBX::Header *header_ptr = reinterpret_cast<UBX::Header *>(&m_rx_buffer.data[0]);
            if (header_ptr->msgClass == UBX::MessageClass::MSG_CLASS_MGA)
            {
                if (header_ptr->msgId == UBX::MGA::ID_DBD)
                {
                    //DEBUG_TRACE("GPS MGA-DBD <-");

                    // Check we have enough space in our buffer to receive this
                    if (m_rx_buffer.len + 1 > sizeof(m_navigation_database) - m_navigation_database_len)
                    {
                        m_navigation_database_len = 0; // Invalidate the data we did receive
                        DEBUG_ERROR("M8QReceiver::fetch_navigation_database: received database is larger than storage buffer");
                        return SendReturnCode::DATA_OVERSIZE;
                    }

                    *write_dst++ = m_rx_buffer.len; // Preface each message with its length for easier resending
                    m_navigation_database_len++;
                    memcpy(write_dst, &m_rx_buffer.data[0], m_rx_buffer.len);
                    write_dst += m_rx_buffer.len;
                    m_navigation_database_len += m_rx_buffer.len;
                    number_of_mga_dbd_msgs++;
                    last_update = rtc->gettime();
                }
                else if (header_ptr->msgId == UBX::MGA::ID_ACK)
                {
                    UBX::MGA::MSG_ACK *msg_ack_ptr = reinterpret_cast<UBX::MGA::MSG_ACK *>(&m_rx_buffer.data[sizeof(UBX::Header)]);
                    //DEBUG_TRACE("GPS MGA-ACK <-");

                    if (msg_ack_ptr->msgPayloadStart == number_of_mga_dbd_msgs)
                    {
                        DEBUG_TRACE("M8QReceiver::fetch_navigation_database: received database of size %lu", m_navigation_database_len);
                        return SendReturnCode::SUCCESS;
                    }
                    else
                    {
                        DEBUG_ERROR("M8QReceiver::fetch_navigation_database: received less MGA-DBD packets then was expected, %lu vs %lu", number_of_mga_dbd_msgs, msg_ack_ptr->msgPayloadStart);
                        m_navigation_database_len = 0; // Invalidate the data we did receive
                        return SendReturnCode::MISSING_DATA;
                    }
                }
            }

            m_rx_buffer.pending = false;
        }

        if (rtc->gettime() - last_update >= timeout)
        {
            DEBUG_ERROR("M8QReceiver::fetch_navigation_database: timed out waiting for response");
            m_navigation_database_len = 0; // Invalidate the data we did receive
            return SendReturnCode::RESPONSE_TIMEOUT;
        }
    }

    return SendReturnCode::SUCCESS;
}

M8QReceiver::SendReturnCode M8QReceiver::send_offline_database()
{
	try {
		LFSFile f(main_filesystem, "gps_config.dat", LFS_O_RDONLY);
		UBXAssistNowOffline ano(f);
		unsigned int i = 0;
	    unsigned int retries = 3;

	    while (ano.value()) {

			// Extract MGA-ANO messages
			UBX::Header *ptr = (UBX::Header *)ano.value();
			unsigned int send_length = sizeof(UBX::Header) + ptr->msgLength + 2;

		    DEBUG_TRACE("M8QReceiver::send_offline_database: sending MGA-ANO#%u sz=%u retries=%u", i, send_length, retries);

			// Send next MGA-ANO message
	        m_rx_buffer.pending = false;
			m_nrf_uart_m8->send((const unsigned char *)ptr, send_length);

			// Wait for the ack, this will only be sent if ackAiding is set in NAVX5
	        auto start_time = rtc->gettime();
	        const std::time_t timeout = 1;

	        for (;;)
	        {
	            if (m_rx_buffer.pending)
	            {
	                UBX::Header *header_ptr = reinterpret_cast<UBX::Header *>(&m_rx_buffer.data[0]);
	                if (header_ptr->msgClass == UBX::MessageClass::MSG_CLASS_MGA && header_ptr->msgId == UBX::MGA::ID_ACK)
	                {
	                    UBX::MGA::MSG_ACK *msg_ack_ptr = reinterpret_cast<UBX::MGA::MSG_ACK *>(&m_rx_buffer.data[sizeof(UBX::Header)]);
	                    DEBUG_TRACE("GPS MGA-ACK <- infoCode: %u", msg_ack_ptr->infoCode);
	                	retries = 3;
	        	        ano.next();
	        	        i++;
	                    break;
	                }

	                m_rx_buffer.pending = false;
	            }

	            if (rtc->gettime() - start_time >= timeout)
	            {
	            	if (--retries == 0) {
						return SendReturnCode::RESPONSE_TIMEOUT;
	            	} else {
	            		break;
	            	}
	            }
	        }

		}
	} catch (...) {
		DEBUG_TRACE("M8QReceiver::send_offline_database: unable to process MGA ANO file");
	}

	return SendReturnCode::SUCCESS;
}

M8QReceiver::SendReturnCode M8QReceiver::send_navigation_database()
{
    if (!m_navigation_database_len)
    {
        DEBUG_TRACE("M8QReceiver::send_navigation_database: no database to send");
        return SendReturnCode::SUCCESS;
    }

    DEBUG_TRACE("M8QReceiver::send_navigation_database: sending database of size %lu", m_navigation_database_len);
    m_capture_messages = true;
    uint8_t *send_ptr = &m_navigation_database[0];
    int32_t bytes_to_process = m_navigation_database_len;
    unsigned int retries = 3;

    while (bytes_to_process > 0 && retries > 0)
    {
        // Fetch the first byte which is the length of the message we are going to send
        uint32_t send_len = *send_ptr++;
        bytes_to_process--;

        // Send the pre-recorded message
        m_rx_buffer.pending = false;
        m_nrf_uart_m8->send(send_ptr, send_len);

        //DEBUG_TRACE("GPS MGA-DBD ->");

        send_ptr += send_len;
        bytes_to_process -= send_len;

        // Wait for the ack, this will only be sent if ackAiding is set in NAVX5
        auto start_time = rtc->gettime();
        const std::time_t timeout = 1;
        for (;;)
        {
            if (m_rx_buffer.pending)
            {
                UBX::Header *header_ptr = reinterpret_cast<UBX::Header *>(&m_rx_buffer.data[0]);
                if (header_ptr->msgClass == UBX::MessageClass::MSG_CLASS_MGA && header_ptr->msgId == UBX::MGA::ID_ACK)
                {
                    //UBX::MGA::MSG_ACK *msg_ack_ptr = reinterpret_cast<UBX::MGA::MSG_ACK *>(&m_rx_buffer.data[sizeof(UBX::Header)]);
                    //DEBUG_TRACE("GPS MGA-ACK <- infoCode: %u", msg_ack_ptr->infoCode);
                	retries = 3;
                    break;
                }

                m_rx_buffer.pending = false;
            }

            if (rtc->gettime() - start_time >= timeout)
            {
            	if (--retries == 0) {
					m_navigation_database_len = 0; // Invalidate the data we did receive
					return SendReturnCode::RESPONSE_TIMEOUT;
            	} else {
            		// Reset pointers back to previous message for a retry
            		bytes_to_process += (send_len + 1);
            		send_ptr -= (send_len + 1);
            		break;
            	}
            }
        }
    }

    return SendReturnCode::SUCCESS;
}

void M8QReceiver::power_on(const GPSNavSettings& nav_settings,
		                   std::function<void(GNSSData data)> data_notification_callback = nullptr)
{
    if (m_state == State::POWERED_ON)
        return;
    
    m_state = State::POWERED_ON;

    DEBUG_INFO("M8QReceiver::power_on");

    m_data_notification_callback = data_notification_callback;
    m_capture_messages = true;
    m_assistnow_autonomous_enable = nav_settings.assistnow_autonomous_enable;
    m_assistnow_offline_enable = nav_settings.assistnow_offline_enable;

    // Clear our dop and pvt message containers
    memset(&m_last_received_pvt, 0, sizeof(m_last_received_pvt));
    memset(&m_last_received_dop, 0, sizeof(m_last_received_dop));
    memset(&m_last_received_status, 0, sizeof(m_last_received_status));

    SendReturnCode ret;

    exit_shutdown_mode();

    ret = setup_uart_port();                  if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}
    ret = setup_gnss_channel_sharing();       if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}
    ret = disable_odometer();                 if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}
    ret = disable_timepulse_output();         if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}
    ret = setup_power_management();           if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}
    ret = setup_lower_power_mode();           if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}
    ret = setup_simple_navigation_settings(nav_settings); if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}
    ret = setup_expert_navigation_settings(); if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}
    ret = supply_time_assistance();			  if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}
    if (m_assistnow_autonomous_enable) {
		ret = send_navigation_database();
		if (ret != SendReturnCode::SUCCESS)
			DEBUG_WARN("M8QReceiver::power_on: failed to send ANA database");
    } else if (!m_assistnow_autonomous_enable && m_assistnow_offline_enable && rtc->is_set()) {
		ret = send_offline_database();
		if (ret != SendReturnCode::SUCCESS)
			DEBUG_WARN("M8QReceiver::power_on: failed to send ANO database");
    }
    ret = enable_nav_pvt_message();           if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}
    ret = enable_nav_status_message();        if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}
    ret = enable_nav_dop_message();           if (ret != SendReturnCode::SUCCESS) {goto POWER_ON_FAILURE;}

    m_capture_messages = false;

    return;

POWER_ON_FAILURE:

    // Our power on has failed so ensure we leave the GPS powered down
    power_off();

    if (ret == SendReturnCode::NACKD)
        throw ErrorCode::GPS_BOOT_NACKED;
    else if (ret == SendReturnCode::RESPONSE_TIMEOUT)
        throw ErrorCode::GPS_BOOT_TIMEOUT;
    else
        throw;
}

void M8QReceiver::reception_callback(uint8_t *data, size_t len)
{
    // If we haven't booted then we need to stash the incoming messages so that we can check for ACK/NACKs whilst configuring the device
    if (m_capture_messages)
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
        // We're looking for a pair of NAV-PVT and NAV-DOP messages so we can retrieve all the data necessary to generate a callback
        // We can use iTow to ensure both messages relate to the same position/time
        UBX::Header *header_ptr = reinterpret_cast<UBX::Header *>(&data[0]);
        if (header_ptr->msgClass == UBX::MessageClass::MSG_CLASS_NAV)
        {
            if (header_ptr->msgId == UBX::NAV::ID_PVT)
            {
                UBX::NAV::PVT::MSG_PVT *msg_pvt_ptr = reinterpret_cast<UBX::NAV::PVT::MSG_PVT *>(&data[sizeof(UBX::Header)]);
                memcpy(&m_last_received_pvt, msg_pvt_ptr, sizeof(m_last_received_pvt));

                //DEBUG_TRACE("GPS NAV-PVT <-");

                populate_gnss_data_and_callback();
            }
            else if (header_ptr->msgId == UBX::NAV::ID_DOP)
            {
                UBX::NAV::DOP::MSG_DOP *msg_dop_ptr = reinterpret_cast<UBX::NAV::DOP::MSG_DOP *>(&data[sizeof(UBX::Header)]);
                memcpy(&m_last_received_dop, msg_dop_ptr, sizeof(m_last_received_dop));

                //DEBUG_TRACE("GPS NAV-DOP <-");

                populate_gnss_data_and_callback();
            }
            else if (header_ptr->msgId == UBX::NAV::ID_STATUS)
            {
                UBX::NAV::STATUS::MSG_STATUS *msg_status_ptr = reinterpret_cast<UBX::NAV::STATUS::MSG_STATUS *>(&data[sizeof(UBX::Header)]);
                memcpy(&m_last_received_status, msg_status_ptr, sizeof(m_last_received_status));

                //DEBUG_TRACE("GPS NAV-STATUS <-");

                populate_gnss_data_and_callback();
            }
        }
    }
}

// Checks if the data we have to date is enough to generate a gnss data callback and if it is calls it
void M8QReceiver::populate_gnss_data_and_callback()
{
    if (m_last_received_pvt.fixType != UBX::NAV::PVT::FIXTYPE_NO &&    // GPS Fix must be achieved
        m_last_received_pvt.valid & UBX::NAV::PVT::VALID_VALID_DATE && // Date must be valid
        m_last_received_pvt.valid & UBX::NAV::PVT::VALID_VALID_TIME && // Time must be valid
        m_last_received_pvt.iTow == m_last_received_dop.iTow &&        // All received DOP, NAV and STATUS,
        m_last_received_status.iTow == m_last_received_dop.iTow)       // must refer to the same position
    {
        GNSSData gnss_data = 
        {
            .iTOW      = m_last_received_pvt.iTow,
            .year      = m_last_received_pvt.year,
            .month     = m_last_received_pvt.month,
            .day       = m_last_received_pvt.day,
            .hour      = m_last_received_pvt.hour,
            .min       = m_last_received_pvt.min,
            .sec       = m_last_received_pvt.sec,
            .valid     = m_last_received_pvt.valid,
            .tAcc      = m_last_received_pvt.tAcc,
            .nano      = m_last_received_pvt.nano,
            .fixType   = m_last_received_pvt.fixType,
            .flags     = m_last_received_pvt.flags,
            .flags2    = m_last_received_pvt.flags2,
            .flags3    = m_last_received_pvt.flags3,
            .numSV     = m_last_received_pvt.numSV,
            .lon       = m_last_received_pvt.lon / 10000000.0,
            .lat       = m_last_received_pvt.lat / 10000000.0,
            .height    = m_last_received_pvt.height,
            .hMSL      = m_last_received_pvt.hMSL,
            .hAcc      = m_last_received_pvt.hAcc,
            .vAcc      = m_last_received_pvt.vAcc,
            .velN      = m_last_received_pvt.velN,
            .velE      = m_last_received_pvt.velE,
            .velD      = m_last_received_pvt.velD,
            .gSpeed    = m_last_received_pvt.gSpeed,
            .headMot   = m_last_received_pvt.headMot / 100000.0f,
            .sAcc      = m_last_received_pvt.sAcc,
            .headAcc   = m_last_received_pvt.headAcc / 100000.0f,
            .pDOP      = m_last_received_dop.pDOP / 100.0f,
            .vDOP      = m_last_received_dop.vDOP / 100.0f,
            .hDOP      = m_last_received_dop.hDOP / 100.0f,
            .headVeh   = m_last_received_pvt.headVeh / 100000.0f,
            .ttff      = m_last_received_status.ttff
        };

        if (m_data_notification_callback)
            m_data_notification_callback(gnss_data);
    }
}

M8QReceiver::SendReturnCode M8QReceiver::setup_uart_port()
{
    // Disable NMEA and only allow UBX messages

	// Make sure we are running at the correct baud rate for the module
	auto ret = sync_baud_rate();
	if (ret != SendReturnCode::SUCCESS)
		return ret;

    CFG::PRT::MSG_UART uart_prt = 
    {
        .portID = CFG::PRT::PORTID_UART,
        .reserved1 = 0,
        .txReady = 0,
        .mode = CFG::PRT::MODE_CHARLEN_8BIT | CFG::PRT::MODE_PARITY_NO | CFG::PRT::MODE_STOP_BITS_1,
        .baudRate = 460800,
        .inProtoMask = CFG::PRT::PROTOMASK_UBX,
        .outProtoMask = CFG::PRT::PROTOMASK_UBX,
        .flags = 0,
        .reserved2 = {0}
    };

    DEBUG_TRACE("M8QReceiver::setup_uart_port: GPS CFG-PRT ->");

    ret = send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_PRT, uart_prt, false);
    m_nrf_uart_m8->change_baudrate(uart_prt.baudRate);

    nrf_delay_ms(100); // Wait for the port to have changed baudrate

    return ret;
}

M8QReceiver::SendReturnCode M8QReceiver::setup_gnss_channel_sharing()
{
    SendReturnCode ret;

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
                // To avoid cross-correlation issues, it is recommended that GPS and QZSS are always both enabled or both disabled
                .flags = CFG::GNSS::FLAGS_QZSS_L1CA | CFG::GNSS::FLAGS_ENABLE
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

    DEBUG_TRACE("M8QReceiver::setup_gnss_channel_sharing: GPS CFG-GNSS ->");
    ret = send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_GNSS, cfg_msg_cfg_gnss);
    if (ret != SendReturnCode::SUCCESS)
        return ret;

    // Applying the GNSS system configuration takes some time. After issuing UBX-
    // CFG-GNSS, wait first for the acknowledgement from the receiver and then 0.5
    // seconds before sending the next command
    nrf_delay_ms(500);

    // Save the configuration up until this point into RAM so that it is retained through the CFG-RST hardware reset
    DEBUG_TRACE("M8QReceiver::setup_gnss_channel_sharing: GPS CFG-CFG ->");
    CFG::CFG::MSG_CFG cfg_msg_cfg_cfg =
    {
        .clearMask   = 0,
        .saveMask    = CFG::CFG::CLEARMASK_IOPORT   |
                       CFG::CFG::CLEARMASK_MSGCONF  |
                       CFG::CFG::CLEARMASK_INFMSG   |
                       CFG::CFG::CLEARMASK_NAVCONF  |
                       CFG::CFG::CLEARMASK_RXMCONF  |
                       CFG::CFG::CLEARMASK_SENCONF  |
                       CFG::CFG::CLEARMASK_RINVCONF |
                       CFG::CFG::CLEARMASK_ANTCONF  |
                       CFG::CFG::CLEARMASK_LOGCONF  |
                       CFG::CFG::CLEARMASK_FTSCONF,
        .loadMask    = 0,
        .deviceMask  = CFG::CFG::DEVMASK_BBR,
    };

    ret = send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_CFG, cfg_msg_cfg_cfg);
    if (ret != SendReturnCode::SUCCESS)
        return ret;

    // If Galileo is enabled, UBX-CFG-GNSS must be followed by UBX-CFG-RST with resetMode set to Hardware reset
    DEBUG_TRACE("M8QReceiver::setup_gnss_channel_sharing: GPS CFG-RST ->");
    CFG::RST::MSG_RST cfg_msg_cfg_rst =
    {
        .navBbrMask = 0x0000,
        .resetMode = CFG::RST::RESETMODE_HARDWARE_RESET_IMMEDIATE,
        .reserved1 = 0,
    };

    ret = send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_RST, cfg_msg_cfg_rst, false); // No acknowledge from this message

    // Wait for the device to have reset
    nrf_delay_ms(1000); // This is an estimated value

    return ret;
}

M8QReceiver::SendReturnCode M8QReceiver::setup_power_management()
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

#if 1 == NO_GPS_POWER_REG
	// Configure EXT_INT pin for power management
    cfg_msg_cfg_pm2.flags |= CFG::PM2::FLAGS_EXTINTWAKE | CFG::PM2::FLAGS_EXTINTBACKUP;
#endif

    DEBUG_TRACE("M8QReceiver::setup_power_management: GPS CFG-PM2 ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_PM2, cfg_msg_cfg_pm2);
}

M8QReceiver::SendReturnCode M8QReceiver::setup_lower_power_mode()
{
    CFG::RXM::MSG_RXM cfg_msg_cfg_rxm =
    {
        .reserved1 = 0,
        .lpMode = CFG::RXM::CONTINUOUS_MODE
    };

    DEBUG_TRACE("M8QReceiver::setup_lower_power_mode: GPS CFG-RXM ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_RXM, cfg_msg_cfg_rxm);
}

M8QReceiver::SendReturnCode M8QReceiver::setup_simple_navigation_settings(const GPSNavSettings& nav_settings)
{
    CFG::NAV5::MSG_NAV5 cfg_msg_cfg_nav5 =
    {
        .mask = CFG::NAV5::MASK_DYN | CFG::NAV5::MASK_MIN_EL | CFG::NAV5::MASK_POS_FIX_MODE | CFG::NAV5::MASK_POS_MASK | CFG::NAV5::MASK_TIME_MASK | CFG::NAV5::MASK_STATIC_HOLD_MASK | CFG::NAV5::MASK_DGPS_MASK | CFG::NAV5::MASK_CNO_THRESHOLD | CFG::NAV5::MASK_UTC,
        .dynModel = (CFG::NAV5::DynModel)nav_settings.dyn_model,
        .fixMode = (CFG::NAV5::FixMode)nav_settings.fix_mode,
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

    DEBUG_TRACE("M8QReceiver::setup_simple_navigation_settings: GPS CFG-NAV5 ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_NAV5, cfg_msg_cfg_nav5);
}

M8QReceiver::SendReturnCode M8QReceiver::setup_expert_navigation_settings()
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
        .aopCfg = CFG::NAVX5::AOPCFG_AOP_ENABLED,
        .reserved7 = {0},
        .aopOrbMaxErr = 100,
        .reserved8 = {0},
        .reserved9 = {0},
        .useAdr = 0
    };

    DEBUG_TRACE("M8QReceiver::setup_expert_navigation_settings: GPS CFG-NAVX5 ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_NAVX5, cfg_msg_cfg_navx5);
}

M8QReceiver::SendReturnCode M8QReceiver::supply_time_assistance()
{
    if (!rtc->is_set())
    {
        DEBUG_TRACE("M8QReceiver::supply_time_assistance: no time to send");
        return SendReturnCode::SUCCESS;
    }

    uint16_t year;
    uint8_t month, day, hour, min, sec;

    convert_datetime_to_epoch(rtc->gettime(), year, month, day, hour, min, sec);

    MGA::MSG_INI_TIME_UTC cfg_msg_ini_time_utc =
    {
        .type = 0x10,
        .version = 0x00,
        .ref = 0x00,
        .leapSecs = -128, // Number of leap seconds unknown
        .year = year,
        .month = month,
        .day = day,
        .hour = hour,
        .minute = min,
        .second = sec,
        .reserved1 = 0,
        .ns = 0,
        .tAccS = 2, // Accurate to within 2 seconds, perhaps this can be improved?
        .reserved2 = {0},
        .tAccNs = 0
    };

    DEBUG_TRACE("M8QReceiver::supply_time_assistance: GPS MGA-INI-TIME-UTC ->");

    // This message expects a MGA-ACK as opposed to a ACK-ACK so we can't use this function for testing the response
    SendReturnCode ret = send_packet_contents(MessageClass::MSG_CLASS_MGA, MGA::ID_INI_TIME_UTC, cfg_msg_ini_time_utc, false);
    if (ret != SendReturnCode::SUCCESS)
        return ret;

    // Instead we shall do so manually (Note: this will only work if ackAiding has been set in NAVX5)
    auto start_time = rtc->gettime();
    const std::time_t timeout = 3;
    for (;;)
    {
        if (m_rx_buffer.pending)
        {
            UBX::Header *header_ptr = reinterpret_cast<UBX::Header *>(&m_rx_buffer.data[0]);
            if (header_ptr->msgClass == UBX::MessageClass::MSG_CLASS_MGA && header_ptr->msgId == UBX::MGA::ID_ACK)
            {
                UBX::MGA::MSG_ACK *msg_ack_ptr = reinterpret_cast<UBX::MGA::MSG_ACK *>(&m_rx_buffer.data[sizeof(UBX::Header)]);
                if (msg_ack_ptr->msgId == MGA::ID_INI_TIME_UTC)
                {
                    DEBUG_TRACE("GPS MGA-ACK <- infoCode: %u", msg_ack_ptr->infoCode);
                    return SendReturnCode::SUCCESS;
                }
            }

            m_rx_buffer.pending = false;
        }

        if (rtc->gettime() - start_time >= timeout)
        {
            DEBUG_ERROR("M8QReceiver::send_navigation_database: timed out waiting for response");
            m_navigation_database_len = 0; // Invalidate the data we did receive
            return SendReturnCode::RESPONSE_TIMEOUT;
        }
    }

    return SendReturnCode::SUCCESS;
}

M8QReceiver::SendReturnCode M8QReceiver::disable_odometer()
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

    DEBUG_TRACE("M8QReceiver::disable_odometer: GPS CFG-ODO ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_ODO, cfg_msg_cfg_odo);
}

M8QReceiver::SendReturnCode M8QReceiver::disable_timepulse_output()
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

    DEBUG_TRACE("M8QReceiver::disable_timepulse_output: GPS CFG-TP5 ->");

    auto ret = send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_TP5, cfg_msg_cfg_tp5);
    if (ret != SendReturnCode::SUCCESS)
        return ret;

    // Disable timepulse 2

    cfg_msg_cfg_tp5.tpIdx = 1;

    DEBUG_TRACE("M8QReceiver::disable_timepulse_output: GPS CFG-TP5 ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_TP5, cfg_msg_cfg_tp5);
}

M8QReceiver::SendReturnCode M8QReceiver::enable_nav_pvt_message()
{
    CFG::MSG::MSG_MSG cfg_msg_nav_pvt =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_PVT,
        .rate = 1
    };

    DEBUG_TRACE("M8QReceiver::enable_nav_pvt_message: GPS CFG-MSG ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_pvt);
}

M8QReceiver::SendReturnCode M8QReceiver::enable_nav_dop_message()
{
    CFG::MSG::MSG_MSG cfg_msg_nav_dop =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_DOP,
        .rate = 1
    };

    DEBUG_TRACE("M8QReceiver::enable_nav_dop_message: GPS CFG-MSG ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_dop);
}

M8QReceiver::SendReturnCode M8QReceiver::enable_nav_status_message()
{
    CFG::MSG::MSG_MSG cfg_msg_nav_dop =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_STATUS,
        .rate = 1
    };

    DEBUG_TRACE("M8QReceiver::enable_nav_status_message: GPS CFG-MSG ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_dop);
}

M8QReceiver::SendReturnCode M8QReceiver::disable_nav_pvt_message()
{
    CFG::MSG::MSG_MSG cfg_msg_nav_pvt =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_PVT,
        .rate = 0
    };

    DEBUG_TRACE("M8QReceiver::disable_nav_pvt_message: GPS CFG-MSG ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_pvt);
}

M8QReceiver::SendReturnCode M8QReceiver::disable_nav_dop_message()
{
    CFG::MSG::MSG_MSG cfg_msg_nav_dop =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_DOP,
        .rate = 0
    };

    DEBUG_TRACE("M8QReceiver::disable_nav_dop_message: GPS CFG-MSG ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_dop);
}

M8QReceiver::SendReturnCode M8QReceiver::disable_nav_status_message()
{
    CFG::MSG::MSG_MSG cfg_msg_nav_dop =
    {
        .msgClass = MessageClass::MSG_CLASS_NAV,
        .msgID = NAV::ID_STATUS,
        .rate = 0
    };

    DEBUG_TRACE("M8QReceiver::disable_nav_status_message: GPS CFG-MSG ->");

    return send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_nav_dop);
}

M8QReceiver::SendReturnCode M8QReceiver::sync_baud_rate()
{
#if 0 == NO_GPS_POWER_REG
	// Power regulator present so just set to initial baud rate
    m_nrf_uart_m8->change_baudrate(9600);
    return SendReturnCode::SUCCESS;
#else

    std::array<unsigned int, 2> baud_rate = { 460800, 9600 };

    for (unsigned int i = 0; i < baud_rate.size(); i++)
    {
    	/* Employ N attempts, the first can be used to wake-up the device */
    	for (unsigned int j = 0; j < 3; j++)
    	{
    		DEBUG_TRACE("M8QReceiver::sync_baud_rate: baud=%u", baud_rate.at(i));
    		m_nrf_uart_m8->change_baudrate(baud_rate.at(i));

    		// This message forces a ACKNAK response
    		CFG::MSG::MSG_MSG_NORATE cfg_msg_invalid =
    	    {
    	        .msgClass = MessageClass::MSG_CLASS_BAD,
    	        .msgID = 0,
    	    };

    		DEBUG_TRACE("M8QReceiver::sync_baud_rate: GPS CFG-MSG ->");

    	    if (SendReturnCode::NACKD == send_packet_contents(MessageClass::MSG_CLASS_CFG, CFG::ID_MSG, cfg_msg_invalid))
    	    	return SendReturnCode::SUCCESS;
    	}
    }

    return SendReturnCode::RESPONSE_TIMEOUT;

#endif
}

M8QReceiver::SendReturnCode M8QReceiver::enter_shutdown_mode()
{
#if 0 == NO_GPS_POWER_REG
    // Disable the power supply for the GPS
    GPIOPins::clear(BSP::GPIO::GPIO_GPS_PWR_EN);
#else
	// Use GPIO_GPS_EXT_INT as a shutdown
    GPIOPins::clear(BSP::GPIO::GPIO_GPS_EXT_INT);
#endif

    if (m_nrf_uart_m8) {
		delete m_nrf_uart_m8;
		m_nrf_uart_m8 = nullptr; // Invalidate this pointer so if we call this function again it doesn't call delete on an invalid pointer
    }

    return SendReturnCode::SUCCESS;
}

M8QReceiver::SendReturnCode M8QReceiver::exit_shutdown_mode()
{
	// Configure UART if not already done
    if (!m_nrf_uart_m8)
        m_nrf_uart_m8 = new NrfUARTM8(UART_GPS, [this](uint8_t *data, size_t len) { reception_callback(data, len); });

#if 0 == NO_GPS_POWER_REG
    // Enable the power supply for the GPS
    GPIOPins::set(BSP::GPIO::GPIO_GPS_PWR_EN);
    nrf_delay_ms(1000); // Necessary to allow the device to boot
#else
	// Use GPIO_GPS_EXT_INT as a wake-up
    GPIOPins::set(BSP::GPIO::GPIO_GPS_EXT_INT);
#endif

    return SendReturnCode::SUCCESS;
}
