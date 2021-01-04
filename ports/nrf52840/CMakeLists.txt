cmake_minimum_required(VERSION 3.1)

project(GenTracker
    VERSION 0.1
    LANGUAGES CXX C ASM
)

set(VERSION_NUMBER 0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDK_ROOT      drivers/nRF5_SDK_17.0.2)
set(PROJ_DIR      .)
set(CORE_DIR      ../../core)
set(LIB_DIR       ../../libraries)

add_executable(${PROJECT_NAME})
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SDK_ROOT}/components/ble/ble_services/ble_ancs_c
    ${SDK_ROOT}/components/ble/ble_services/ble_ias_c
    ${SDK_ROOT}/components/ble/ble_services/ble_gls
    ${SDK_ROOT}/components/libraries/bootloader/ble_dfu
    ${SDK_ROOT}/components/ble/ble_advertising
    ${SDK_ROOT}/components/ble/ble_services/ble_bas_c
    ${SDK_ROOT}/components/ble/ble_services/ble_hrs_c
    ${SDK_ROOT}/components/ble/ble_dtm
    ${SDK_ROOT}/components/ble/ble_services/ble_rscs_c
    ${SDK_ROOT}/components/ble/common
    ${SDK_ROOT}/components/ble/ble_services/ble_lls
    ${SDK_ROOT}/components/ble/ble_services/ble_bas
    ${SDK_ROOT}/components/ble/ble_services/ble_ans_c
    ${SDK_ROOT}/components/ble/ble_services/ble_nus_c
    ${SDK_ROOT}/components/ble/ble_services/ble_ias
    ${SDK_ROOT}/components/nfc/ndef/conn_hand_parser/ble_oob_advdata_parser
    ${SDK_ROOT}/components/ble/ble_services/ble_dfu
    ${SDK_ROOT}/components/ble/ble_services/ble_lbs
    ${SDK_ROOT}/components/ble/ble_services/ble_hts
    ${SDK_ROOT}/components/ble/ble_services/ble_cts_c
    ${SDK_ROOT}/components/ble/ble_services/ble_cscs
    ${SDK_ROOT}/components/ble/ble_link_ctx_manager
    ${SDK_ROOT}/components/ble/ble_services/ble_nus
    ${SDK_ROOT}/components/ble/ble_services/ble_hids
    ${SDK_ROOT}/components/nfc/ndef/connection_handover/ble_oob_advdata
    ${SDK_ROOT}/components/nfc/ndef/connection_handover/ble_pair_msg
    ${SDK_ROOT}/components/ble/peer_manager
    ${SDK_ROOT}/components/ble/ble_services/ble_tps
    ${SDK_ROOT}/components/ble/ble_services/ble_dis
    ${SDK_ROOT}/components/ble/nrf_ble_gatt
    ${SDK_ROOT}/components/ble/nrf_ble_qwr
    ${SDK_ROOT}/components/ble/ble_services/ble_lbs_c
    ${SDK_ROOT}/components/nfc/ndef/connection_handover/ble_pair_lib
    ${SDK_ROOT}/components/ble/ble_racp
    ${SDK_ROOT}/components/ble/ble_services/ble_hrs
    ${SDK_ROOT}/components/ble/ble_services/ble_rscs
    ${SDK_ROOT}/components/libraries/timer
    ${SDK_ROOT}/components/libraries/sortlist
    ${SDK_ROOT}/components/libraries/atomic
    ${SDK_ROOT}/components/libraries/pwr_mgmt
    ${SDK_ROOT}/components/libraries/atomic_flags
    ${SDK_ROOT}/components/libraries/atomic_fifo
    ${SDK_ROOT}/components/libraries/strerror
    ${SDK_ROOT}/components/libraries/mutex
    ${SDK_ROOT}/external/fprintf
    ${SDK_ROOT}/components/libraries/memobj
    ${SDK_ROOT}/components/libraries/balloc
    ${SDK_ROOT}/components/libraries/ringbuf
    ${SDK_ROOT}/components/toolchain/cmsis/include
    ${SDK_ROOT}/components/libraries/delay
    ${SDK_ROOT}/components/libraries/util
    ${SDK_ROOT}/components/libraries/log
    ${SDK_ROOT}/components/libraries/log/src
    ${SDK_ROOT}/components/libraries/experimental_section_vars
    ${SDK_ROOT}/components/softdevice/common
    ${SDK_ROOT}/components/softdevice/s140/headers
    ${SDK_ROOT}/components/softdevice/s140/headers/nrf52
    ${SDK_ROOT}/modules/nrfx
    ${SDK_ROOT}/modules/nrfx/hal
    ${SDK_ROOT}/modules/nrfx/mdk
    ${SDK_ROOT}/modules/nrfx/drivers/include
    ${SDK_ROOT}/integration/nrfx
    ${PROJ_DIR}
    ${PROJ_DIR}/drivers/nrf_log_redirect
    ${PROJ_DIR}/bsp/horizon_v4.0
    ${LIB_DIR}/littlefs
    ${CORE_DIR}/hardware
    ${CORE_DIR}/configuration
    ${CORE_DIR}/filesystem
    ${CORE_DIR}/interface
    ${CORE_DIR}/protocol
    ${CORE_DIR}/util
    ${CORE_DIR}/logging
    ${CORE_DIR}/sm
    #${CORE_DIR}/scheduling
    ${PROJ_DIR}/core/hardware
    ${PROJ_DIR}/core/interface
    ${PROJ_DIR}/core/filesystem
)

target_sources(${PROJECT_NAME} PRIVATE
    ${LIB_DIR}/littlefs/lfs_util.c
    ${LIB_DIR}/littlefs/lfs.c
    ${SDK_ROOT}/components/ble/common/ble_advdata.c
    ${SDK_ROOT}/components/ble/ble_advertising/ble_advertising.c
    ${SDK_ROOT}/components/ble/common/ble_conn_params.c
    ${SDK_ROOT}/components/ble/common/ble_conn_state.c
    ${SDK_ROOT}/components/ble/ble_link_ctx_manager/ble_link_ctx_manager.c
    ${SDK_ROOT}/components/ble/common/ble_srv_common.c
    ${SDK_ROOT}/components/ble/nrf_ble_gatt/nrf_ble_gatt.c
    ${SDK_ROOT}/components/ble/nrf_ble_qwr/nrf_ble_qwr.c
    ${SDK_ROOT}/components/ble/ble_services/ble_nus/ble_nus.c
    ${SDK_ROOT}/components/libraries/memobj/nrf_memobj.c
    ${SDK_ROOT}/components/libraries/balloc/nrf_balloc.c
    ${SDK_ROOT}/components/libraries/log/src/nrf_log_frontend.c
    ${SDK_ROOT}/components/libraries/log/src/nrf_log_str_formatter.c
    ${SDK_ROOT}/components/libraries/ringbuf/nrf_ringbuf.c
    ${SDK_ROOT}/components/libraries/log/src/nrf_log_backend_serial.c
    ${SDK_ROOT}/external/fprintf/nrf_fprintf.c
    ${SDK_ROOT}/external/fprintf/nrf_fprintf_format.c
    ${SDK_ROOT}/components/libraries/experimental_section_vars/nrf_section_iter.c
    ${SDK_ROOT}/components/libraries/util/app_util_platform.c
    ${SDK_ROOT}/components/libraries/atomic_flags/nrf_atflags.c
    ${SDK_ROOT}/components/libraries/atomic/nrf_atomic.c
    ${SDK_ROOT}/components/libraries/timer/app_timer2.c
    ${SDK_ROOT}/components/libraries/atomic_fifo/nrf_atfifo.c
    ${SDK_ROOT}/components/libraries/util/app_error.c
    ${SDK_ROOT}/components/libraries/util/app_error_handler_gcc.c
    ${SDK_ROOT}/components/libraries/util/app_error_weak.c
    ${SDK_ROOT}/components/libraries/strerror/nrf_strerror.c
    ${SDK_ROOT}/components/libraries/timer/drv_rtc.c
    ${SDK_ROOT}/components/libraries/pwr_mgmt/nrf_pwr_mgmt.c
    ${SDK_ROOT}/components/libraries/sortlist/nrf_sortlist.c
    ${SDK_ROOT}/components/softdevice/common/nrf_sdh.c
    ${SDK_ROOT}/components/softdevice/common/nrf_sdh_ble.c
    ${SDK_ROOT}/components/softdevice/common/nrf_sdh_soc.c
    ${SDK_ROOT}/modules/nrfx/mdk/gcc_startup_nrf52840.S
    ${SDK_ROOT}/modules/nrfx/mdk/system_nrf52840.c
    ${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_uarte.c
    ${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_qspi.c
    ${PROJ_DIR}/bsp/horizon_v4.0/bsp.cpp
    ${PROJ_DIR}/core/filesystem/is25_flash_file_system.cpp
    ${PROJ_DIR}/core/interface/ble_interface.cpp
    ${PROJ_DIR}/drivers/nrf_log_redirect/nrf_log_redirect.c
    ${PROJ_DIR}/main.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    DEBUG
    DEBUG_ENABLE
    APP_TIMER_V2
    APP_TIMER_V2_RTC1_ENABLED
    BOARD_PCA10059
    CONFIG_GPIO_AS_PINRESET
    CONFIG_NFCT_PINS_AS_GPIOS
    FLOAT_ABI_HARD
    NRF52840_XXAA
    NRF_SD_BLE_API_VERSION=7
    S140
    SOFTDEVICE_PRESENT
    __HEAP_SIZE=8192
    __STACK_SIZE=65536
    #LFS_YES_TRACE
    ARGOS_FREQ_MIN_WORKAROUND
    OLD_ZONE_FORMAT_WORKAROUND
)

target_compile_options(${PROJECT_NAME} PRIVATE
    # Warnings
    -Wall
    -Wextra
    -Werror
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wnull-dereference
    -Wshadow
    -Wpointer-arith
    -Wdangling-else
    -Wrestrict
    -Wdouble-promotion
    -Wvla
    -Wswitch-enum
    -Wswitch-default
    -Winvalid-pch
    -Wodr

    # C Only Warnings
    $<$<COMPILE_LANGUAGE:C>:${-Wjump-misses-init}>

    # Disabled Warnings
    -Wno-unused-parameter
    -Wno-expansion-to-defined # The NRFX library requires this
    -Wno-psabi
    -Wno-unused-function

    # Build flags
    -mcpu=cortex-m4
    -mthumb
    -mabi=aapcs
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    -ffunction-sections
    -fdata-sections
    -fno-strict-aliasing
    -fno-builtin
    -fshort-enums

    # Always output coloured text from GCC
    -fdiagnostics-color=always
)

target_link_options(${PROJECT_NAME} PRIVATE
    -L${CMAKE_SOURCE_DIR}/${SDK_ROOT}/modules/nrfx/mdk
    -T${PROJECT_SOURCE_DIR}/gcc_nrf52840.ld
    -mcpu=cortex-m4
    -mthumb
    -mabi=aapcs
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    -Wl,--gc-sections
    #--specs=nano.specs # We use exceptions so we can not use this due to nano-specs being built with exceptions disabled
    "-u _printf_float"
    "-u _scanf_float"
    -Xlinker -Map=${PROJECT_NAME}.map
)

target_link_libraries(${PROJECT_NAME}
    m
    c
    nosys
)

function(create_dfu executable)
    add_custom_command(
        TARGET ${executable}
        POST_BUILD
        COMMENT "Generating ${executable}_dfu.zip"
        COMMAND nrfutil pkg generate --hw-version 52 --sd-req 0xAE,0xB6 --application-version ${VERSION_NUMBER} --application ${CMAKE_CURRENT_BINARY_DIR}/${executable}.hex --key-file ${PROJECT_SOURCE_DIR}/${PROJ_DIR}/nrfutil_pkg_key.pem ${CMAKE_CURRENT_BINARY_DIR}/${executable}_dfu.zip
    )
endfunction(create_dfu)

create_bin(${PROJECT_NAME})
create_hex(${PROJECT_NAME})
create_dfu(${PROJECT_NAME})

add_custom_target(flash
                  COMMENT "Flashing the softdevice & firmware"
                  COMMAND nrfjprog -f nrf52 --eraseall
                  COMMAND nrfjprog -f nrf52 --program ${PROJECT_SOURCE_DIR}/${SDK_ROOT}/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --sectoranduicrerase
                  COMMAND nrfjprog -f nrf52 --program ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex --sectorerase
                  COMMAND nrfjprog -f nrf52 --reset
)
add_dependencies(flash ${PROJECT_NAME})