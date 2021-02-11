#include <stdint.h>
#include <string.h>

#include "nrf_uart_m8.hpp"
#include "interrupt_lock.hpp"
#include "bsp.hpp"
#include "debug.hpp"

NrfUARTM8::NrfUARTM8(unsigned int instance, std::function<void(uint8_t *data, size_t len)> on_receive)
{
    m_instance = instance;
    m_on_receive = on_receive;
    m_state = WAITING_SYNC_CHAR_1;

    // Copy the configuration and add our object to the context
    nrfx_uarte_config_t config = BSP::UART_Inits[instance].config;
    config.p_context = reinterpret_cast<void *>(this);

    nrfx_uarte_init(&BSP::UART_Inits[instance].uarte, &config, static_event_handler);
    
    // Set up double buffered reception
    nrfx_uarte_rx(&BSP::UART_Inits[instance].uarte, &rx_byte, 1);
}

NrfUARTM8::~NrfUARTM8()
{
    nrfx_uarte_uninit(&BSP::UART_Inits[m_instance].uarte);
}

// Redirects the C style callback to a C++ object
void NrfUARTM8::static_event_handler(nrfx_uarte_event_t const * p_event, void * p_context)
{
    reinterpret_cast<NrfUARTM8 *>(p_context)->event_handler(p_event);
}

void NrfUARTM8::event_handler(nrfx_uarte_event_t const * p_event)
{
    switch (p_event->type)
    {
        case NRFX_UARTE_EVT_RX_DONE:
        {
            uint8_t received_data = rx_byte;
            nrfx_uarte_rx(&BSP::UART_Inits[m_instance].uarte, &rx_byte, 1); // Keep receiving
            //printf("%c", received_data);
            //printf("%02X ", received_data);
            update_state(received_data);
            break;
        }

        case NRFX_UARTE_EVT_ERROR:
            nrfx_uarte_rx(&BSP::UART_Inits[m_instance].uarte, &rx_byte, 1); // Keep receiving
            break;

        case NRFX_UARTE_EVT_TX_DONE:
        default:
            break;
    }
}

// Updates our reception state, this should be quick as this is called from an interrupt context
void NrfUARTM8::update_state(uint8_t new_byte)
{
    static uint32_t bytes_to_read = 0;
    static uint32_t bytes_read = 0;

    switch (m_state)
    {
        case WAITING_SYNC_CHAR_1:
            if (new_byte == UBX::SYNC_CHAR1)
            {
                rx_buffer[0] = new_byte;
                m_state = WAITING_SYNC_CHAR_2;
            }
            break;

		case WAITING_SYNC_CHAR_2:
            if (new_byte == UBX::SYNC_CHAR2)
            {
                rx_buffer[1] = new_byte;
                m_state = WAITING_CLASS;
            }
            else
                m_state = WAITING_SYNC_CHAR_1; // Unexpected value so return
            break;

		case WAITING_CLASS:
            rx_buffer[2] = new_byte;
            m_state = WAITING_ID;
            break;

		case WAITING_ID:
            rx_buffer[3] = new_byte;
            m_state = WAITING_LENGTH_LOWER;
            break;

		case WAITING_LENGTH_LOWER:
            rx_buffer[4] = new_byte;
            m_state = WAITING_LENGTH_UPPER;
            break;

		case WAITING_LENGTH_UPPER:
            rx_buffer[5] = new_byte;
            bytes_to_read = reinterpret_cast<UBX::Header*>(&rx_buffer[0])->msgLength + 2; // Add two for the checksum at the end
            bytes_read = 0;
            if (bytes_to_read >= rx_buffer.max_size() + sizeof(UBX::Header) + 2)
            {
                // A length this large would overrun our buffer so discard it
                m_state = WAITING_SYNC_CHAR_1;
            }
            else
                m_state = WAITING_PAYLOAD_AND_CHECKSUM;
            break;

		case WAITING_PAYLOAD_AND_CHECKSUM:
            rx_buffer[6 + bytes_read] = new_byte;
            bytes_read++;
            if (bytes_read >= bytes_to_read)
            {

                // All the expected bytes were received
                // Check the checksum was valid and if not then discard this message
                UBX::Header *header_ptr = reinterpret_cast<UBX::Header*>(&rx_buffer[0]);
                uint8_t *checksum_ptr = reinterpret_cast<uint8_t *>(&header_ptr->msgClass);
                size_t bytes_to_check = header_ptr->msgLength + 4; // Add four for Class, ID and Length

                uint8_t ck[2] = {0};
                for (unsigned int i = 0; i < bytes_to_check; i++)
                {
                    ck[0] = ck[0] + checksum_ptr[i];
                    ck[1] = ck[1] + ck[0];
                }

                if (ck[0] == rx_buffer[sizeof(UBX::Header) + header_ptr->msgLength] &&
                    ck[1] == rx_buffer[sizeof(UBX::Header) + header_ptr->msgLength + 1])
                {
                    if (m_on_receive)
                        m_on_receive(&rx_buffer[0], bytes_read + sizeof(UBX::Header));
                }
                else
                {
                    DEBUG_WARN("GPS Checksum invalid");
                }              
                
                m_state = WAITING_SYNC_CHAR_1;
            }
            break;

        default:
            break;
    }
}

int NrfUARTM8::send(const uint8_t * data, uint32_t size)
{
    nrfx_err_t ret = nrfx_uarte_tx(&BSP::UART_Inits[m_instance].uarte, data, size);
    if (ret != NRFX_SUCCESS)
        return ret;

    // nrfx_uarte_tx is non-blocking so we need to make it blocking so our data buffer remains valid for the duration of the transfer
    while (nrfx_uarte_tx_in_progress(&BSP::UART_Inits[m_instance].uarte))
    {}

    return size;
}
