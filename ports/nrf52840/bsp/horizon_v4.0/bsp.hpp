#pragma once

#include "sdk_config.h"

namespace BSP
{

    enum class
    {
    #ifdef NRFX_QSPI_ENABLED
        QSPI_0,
    #endif
        QSPI_TOTAL_NUMBER
    } QSPI;

    typedef struct
    {
        nrfx_qspi_config_t qspi_config;
    } QSPI_InitTypeDefAndInst_t;

    extern QSPI_InitTypeDefAndInst_t QSPI_Inits[QSPI_TOTAL_NUMBER];

}