#ifndef NRF_UARTE_H__
#define NRF_UARTE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef void *NRF_UARTE_Type;

typedef enum
{
    NRF_UARTE_BAUDRATE_1200,
    NRF_UARTE_BAUDRATE_2400,
    NRF_UARTE_BAUDRATE_4800,
    NRF_UARTE_BAUDRATE_9600,
    NRF_UARTE_BAUDRATE_14400,
    NRF_UARTE_BAUDRATE_19200,
    NRF_UARTE_BAUDRATE_28800,
    NRF_UARTE_BAUDRATE_31250,
    NRF_UARTE_BAUDRATE_38400,
    NRF_UARTE_BAUDRATE_56000,
    NRF_UARTE_BAUDRATE_57600,
    NRF_UARTE_BAUDRATE_76800,
    NRF_UARTE_BAUDRATE_115200,
    NRF_UARTE_BAUDRATE_230400,
    NRF_UARTE_BAUDRATE_250000,
    NRF_UARTE_BAUDRATE_460800,
    NRF_UARTE_BAUDRATE_921600,
    NRF_UARTE_BAUDRATE_1000000
} nrf_uarte_baudrate_t;

typedef struct {
    NRF_UARTE_Type *uarte;
} nrf_uarte_t;

void nrf_uarte_baudrate_set(NRF_UARTE_Type   * p_reg, nrf_uarte_baudrate_t baudrate);

#ifdef __cplusplus
}
#endif

#endif //NRF_UARTE_H__
