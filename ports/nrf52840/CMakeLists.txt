cmake_minimum_required(VERSION 3.1)

project(LinkIt
    VERSION 0.1
    LANGUAGES CXX C ASM
)

set(VERSION_NUMBER 0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDK_ROOT      drivers/nRF5_SDK_17.0.2)
set(PROJ_DIR      .)
set(CORE_DIR      ../../core)
set(LIB_DIR       ../../libraries)

option(BOARD "Board to compile for" NONE)

if(${MODEL} MATCHES SB OR ${MODEL} MATCHES UW OR ${MODEL} MATCHES CORE)
    set(MODEL_SB 0)
    set(MODEL_UW 0)
    set(MODEL_CORE 0)
    set(DEVICE_MODEL "Linkit ${MODEL}")
    set(HAS_WCHG 0)
    if(${MODEL} MATCHES SB)
        set(MODEL_SB 1)
    elseif(${MODEL} MATCHES UW)
        set(MODEL_UW 1)
	    set(HAS_WCHG 1)
    elseif(${MODEL} MATCHES CORE)
        set(MODEL_CORE 1)
	    set(HAS_WCHG 1)
    endif()
else()
    message(FATAL_ERROR "MODEL must be one of {SB, CORE, UW}")
endif()

# Append board model to the target name
if(${BOARD} MATCHES HORIZON)
    set(TARGET_NAME ${PROJECT_NAME}_horizon_board)
    set(BOARD_DIR     "${PROJ_DIR}/bsp/horizon_v4.0")
    set(DEVICE_MODEL "Horizon CORE")
    set(MODEL_SB 0)
    set(MODEL_UW 0)
    set(MODEL_CORE 1)
    set(HAS_WCHG 0)
    set(BOOTLOADER "bootloader/gentracker_secure_bootloader/horizon_v4.0/armgcc/_build/horizon_bootloader_v4_linkit_merged.hex")
elseif(${BOARD} MATCHES LINKIT)
    set(TARGET_NAME ${PROJECT_NAME}_${MODEL}_board)
    set(BOARD_DIR     "${PROJ_DIR}/bsp/gentracker_v1.0")
    set(BOOTLOADER "bootloader/gentracker_secure_bootloader/gentracker_v1.0/armgcc/_build/cls_bootloader_v1_linkit_merged.hex")
else()
    message(FATAL_ERROR "Board must be HORIZON or LINKIT")
endif()

if (DEBUG_LEVEL)
else()
    set(DEBUG_LEVEL 3)
endif()


execute_process(
        COMMAND git describe --dirty --long
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE FW_APP_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )

add_executable(${TARGET_NAME})
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

target_include_directories(${TARGET_NAME} SYSTEM PRIVATE
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
    ${SDK_ROOT}/components/libraries/crc32
    ${SDK_ROOT}/components/libraries/crc16
	${SDK_ROOT}/components/libraries/queue
    ${SDK_ROOT}/components/libraries/libuarte
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
    ${BOARD_DIR}
    ${LIB_DIR}/etl-20.27.3/include
    ${LIB_DIR}/inplace_function
    ${LIB_DIR}/littlefs
    ${LIB_DIR}/CmBacktrace/cm_backtrace
    ${LIB_DIR}/prepass/prepass_package_code-PREPAS_V3.4
    ${CORE_DIR}/ble_services
    ${CORE_DIR}/hardware
    ${CORE_DIR}/services
    ${CORE_DIR}/configuration
    ${CORE_DIR}/filesystem
    ${CORE_DIR}/interface
    ${CORE_DIR}/protocol
    ${CORE_DIR}/util
    ${CORE_DIR}/logging
    ${CORE_DIR}/sm
    ${CORE_DIR}/scheduling
    ${PROJ_DIR}/core/hardware
    ${PROJ_DIR}/core/interface
    ${PROJ_DIR}/core/memmang
    ${PROJ_DIR}/core/filesystem
    ${PROJ_DIR}/core/scheduling
    ${PROJ_DIR}/core/hardware/artic_sat
    ${PROJ_DIR}/core/hardware/m8qasync
    ${PROJ_DIR}/core/hardware/ms58xx
    ${PROJ_DIR}/core/hardware/ad5933
    ${PROJ_DIR}/core/hardware/ltr_303
    ${PROJ_DIR}/core/hardware/oem_ph
    ${PROJ_DIR}/core/hardware/oem_rtd
    ${PROJ_DIR}/core/hardware/ezo_rtd
    ${PROJ_DIR}/core/hardware/cdt
    ${PROJ_DIR}/core/hardware/bmx160
    ${PROJ_DIR}/core/hardware/stwlc68_otp
    ${PROJ_DIR}/core/hardware/run_cam
)

target_sources(${TARGET_NAME} PRIVATE
    ${LIB_DIR}/littlefs/lfs_util.c
    ${LIB_DIR}/littlefs/lfs.c
    ${LIB_DIR}/prepass/prepass_package_code-PREPAS_V3.4/previpass.c
    ${LIB_DIR}/prepass/prepass_package_code-PREPAS_V3.4/previpass_util.c
    ${LIB_DIR}/CmBacktrace/cm_backtrace/cm_backtrace.c
    ${SDK_ROOT}/components/ble/common/ble_advdata.c
    ${SDK_ROOT}/components/ble/ble_advertising/ble_advertising.c
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
    ${SDK_ROOT}/components/libraries/crc32/crc32.c
    ${SDK_ROOT}/components/libraries/crc16/crc16.c
	${SDK_ROOT}/components/libraries/queue/nrf_queue.c
    ${SDK_ROOT}/components/libraries/libuarte/nrf_libuarte_async.c
    ${SDK_ROOT}/components/libraries/libuarte/nrf_libuarte_drv.c
    ${SDK_ROOT}/modules/nrfx/mdk/system_nrf52840.c
    ${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_uarte.c
    ${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_qspi.c
    ${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_spim.c
    ${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_twim.c
    ${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_rtc.c
    ${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_wdt.c
	${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_ppi.c
	${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_timer.c
    ${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_gpiote.c
    ${SDK_ROOT}/modules/nrfx/drivers/src/nrfx_saadc.c
    ${SDK_ROOT}/modules/nrfx/hal/nrf_nvmc.c
    ${CORE_DIR}/hardware/sensor.cpp
    ${CORE_DIR}/hardware/reed.cpp
    ${CORE_DIR}/sm/gentracker.cpp
    ${CORE_DIR}/sm/ledsm.cpp
    ${CORE_DIR}/services/service.cpp
    ${CORE_DIR}/services/gps_service.cpp
    ${CORE_DIR}/services/cam_service.cpp
    ${CORE_DIR}/services/argos_tx_service.cpp
    ${CORE_DIR}/services/argos_rx_service.cpp
    ${CORE_DIR}/services/uwdetector_service.cpp
    ${CORE_DIR}/logging/logger.cpp
    ${CORE_DIR}/util/haversine.cpp
    ${CORE_DIR}/filesystem/ota_flash_file_updater.cpp
    ${CORE_DIR}/protocol/dte_commands.cpp
    ${CORE_DIR}/protocol/dte_params.cpp
    ${CORE_DIR}/configuration/calibration.cpp
    ${BOARD_DIR}/bsp.cpp
    ${PROJ_DIR}/core/filesystem/is25_flash.cpp
    ${PROJ_DIR}/core/hardware/ms58xx/ms58xx.cpp
    ${PROJ_DIR}/core/hardware/ad5933/ad5933.cpp
    ${PROJ_DIR}/core/hardware/bmx160/bmx160.cpp
    ${PROJ_DIR}/core/hardware/bmx160/bmx160.c
    ${PROJ_DIR}/core/hardware/cdt/cdt.cpp
    ${PROJ_DIR}/core/hardware/ltr_303/ltr_303.cpp
    ${PROJ_DIR}/core/hardware/oem_ph/oem_ph.cpp
    ${PROJ_DIR}/core/hardware/oem_rtd/oem_rtd.cpp
    ${PROJ_DIR}/core/hardware/ezo_rtd/ezo_rtd.cpp
    ${PROJ_DIR}/core/hardware/artic_sat/artic_sat.cpp
    ${PROJ_DIR}/core/hardware/artic_sat/pa_driver.cpp
	${PROJ_DIR}/core/hardware/m8qasync/m8qasync.cpp
    ${PROJ_DIR}/core/hardware/m8qasync/ubx_comms.cpp
    ${PROJ_DIR}/core/hardware/run_cam/runcam.cpp
    ${PROJ_DIR}/core/hardware/nrf_timer.cpp
    ${PROJ_DIR}/core/hardware/nrf_rtc.cpp
    ${PROJ_DIR}/core/hardware/nrf_spim.cpp
    ${PROJ_DIR}/core/hardware/nrf_gpio.cpp
    ${PROJ_DIR}/core/hardware/nrf_pmu.cpp
    ${PROJ_DIR}/core/hardware/nrf_switch.cpp
    ${PROJ_DIR}/core/hardware/nrf_irq.cpp
    ${PROJ_DIR}/core/hardware/nrf_battery_mon.cpp
    ${PROJ_DIR}/core/hardware/nrf_i2c.cpp
    ${PROJ_DIR}/core/interface/ble_interface.cpp
    ${PROJ_DIR}/core/interface/ble_stm_ota.c
    ${PROJ_DIR}/core/memmang/heap_4.c
    ${PROJ_DIR}/core/memmang/heap_mem.c
    ${PROJ_DIR}/core/memmang/bindings.c
    ${PROJ_DIR}/core/memmang/bindings.cpp
    ${PROJ_DIR}/core/memmang/memmang.cpp
    ${PROJ_DIR}/core/scheduling/interrupt_lock.cpp
    ${PROJ_DIR}/drivers/nrf_log_redirect/nrf_log_redirect.c
    ${PROJ_DIR}/gcc_startup_nrf52840.S
    ${PROJ_DIR}/main.cpp
)

if (HAS_WCHG)
        target_sources(${TARGET_NAME} PRIVATE
		    ${PROJ_DIR}/core/hardware/stwlc68_otp/otphal.c
		    ${PROJ_DIR}/core/hardware/stwlc68_otp/otp_chip_info.c
		    ${PROJ_DIR}/core/hardware/stwlc68_otp/stwlc68.cpp
        )
endif()

target_compile_definitions(${TARGET_NAME} PRIVATE
    Release=0
    Debug=1
    HAS_WCHG=${HAS_WCHG}
    MODEL_SB=${MODEL_SB}
    MODEL_UW=${MODEL_UW}
    MODEL_CORE=${MODEL_CORE}
    BUILD_TYPE=${CMAKE_BUILD_TYPE}
    #SCHEDULER_DEBUG
    DEBUG_ENABLE=1
    DEBUG_LEVEL=${DEBUG_LEVEL}
    DEBUG_USING_RTC
    BOARD_PCA10059
    BOARD_${BOARD}
    CONFIG_GPIO_AS_PINRESET
    CONFIG_NFCT_PINS_AS_GPIOS
    FLOAT_ABI_HARD
    NRF52840_XXAA
    NRF_SD_BLE_API_VERSION=7
    S140
    SOFTDEVICE_PRESENT
    __HEAP_SIZE=0 # Use configTOTAL_HEAP_SIZE instead
    __STACK_SIZE=110000
    configTOTAL_HEAP_SIZE=65536
    DEVICE_MODEL_NAME="${DEVICE_MODEL}"s
    #LFS_YES_TRACE
    #BATTERY_NOT_FITTED
    DUMMY_MS58xx
    #DEBUG_AD5933
    DEBUG_TO_SYSTEMLOG
    ETL_LOG_ERRORS
    ARGOS_USE_CRC8
    PREPASS_EMBEDDED
    FW_APP_VERSION_STR="${FW_APP_VERSION}"s
    FW_APP_VERSION_STR_C="${FW_APP_VERSION}"
    #WORKAROUND_ALLCAST_CONSTELLATION_STATUS_ENCODING_BUG
    _XOPEN_SOURCE=700 # Enables strptime() usage
    __STARTUP_FILL_STACK
)

target_compile_options(${TARGET_NAME} PRIVATE
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

    # Debugging stack corruption
    -fstack-protector-strong

    # Optimise for size always
    -Os
    -g3
)

set_source_files_properties(${LIB_DIR}/prepass/prepass_package_code-PREPAS_V3.4/previpass.c PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-switch-enum -Wno-switch")

set_source_files_properties(${LIB_DIR}/CmBacktrace/cm_backtrace/cm_backtrace.c PROPERTIES COMPILE_FLAGS "-Wno-unused-const-variable -Wno-unused-variable")

target_link_options(${TARGET_NAME} PRIVATE
    -L${CMAKE_SOURCE_DIR}/${SDK_ROOT}/modules/nrfx/mdk
    -T${PROJECT_SOURCE_DIR}/gcc_nrf52840.ld
    -mcpu=cortex-m4
    -mthumb
    -mabi=aapcs
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    -Wl,--gc-sections
    #--specs=nano.specs # We use exceptions so we can not use this due to nano-specs being built with exceptions disabled
    -u_printf_float
    -u_scanf_float
    -Xlinker -Map=${TARGET_NAME}.map

    -Wl,--wrap=__stack_chk_fail    
)

target_link_libraries(${TARGET_NAME}
    m
    c
    nosys
)

function(create_dfu executable)
    add_custom_command(
        TARGET ${executable}
        POST_BUILD
        COMMENT "Generating ${executable}_dfu.zip"
        COMMAND nrfutil pkg generate --hw-version 52 --sd-req 0x0100 --application-version 0 --application ${CMAKE_CURRENT_BINARY_DIR}/${executable}.hex --key-file ${PROJECT_SOURCE_DIR}/${PROJ_DIR}/nrfutil_pkg_key.pem ${CMAKE_CURRENT_BINARY_DIR}/${executable}_dfu.zip
    )
endfunction(create_dfu)

function(create_wrapped_bin executable)
    add_custom_command(
        TARGET ${executable}
        POST_BUILD
        COMMENT "Generating ${executable}.img"
        COMMAND printf "%08x" `stat -c %s ${executable}.bin` | xxd -r -p > ${executable}.img && crc32 ${executable}.bin | xxd -r -p >> ${executable}.img && cat ${executable}.bin >> ${executable}.img
    )
endfunction(create_wrapped_bin)

create_bin(${TARGET_NAME})
create_wrapped_bin(${TARGET_NAME})
create_hex(${TARGET_NAME})
create_dfu(${TARGET_NAME})

add_custom_target(flash_softdev
                  COMMENT "Flashing the softdevice using JLink..."
                  COMMAND nrfjprog -f nrf52 --eraseall
                  COMMAND nrfjprog -f nrf52 --program ${PROJECT_SOURCE_DIR}/${SDK_ROOT}/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --sectoranduicrerase
                  COMMAND nrfjprog -f nrf52 --reset
)

add_custom_target(flash
                  COMMENT "Flashing the softdevice & firmware using JLink..."
                  COMMAND nrfjprog -f nrf52 --eraseall
                  COMMAND nrfjprog -f nrf52 --program ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_merged.hex --sectorerase
                  COMMAND nrfjprog -f nrf52 --reset
)

add_custom_target(dfu_app
                  COMMENT "Sending application image over DFU (reset device with DEBUG pin jumpered!)..."
                  COMMAND nrfutil dfu usb-serial -p /dev/ttyACM0 -pkg ${TARGET_NAME}_dfu.zip
)

add_custom_target(merged_hex
                  COMMENT "Create combined bootloader, settings and applications file"
                  COMMAND nrfutil settings generate --family NRF52840 --application ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.hex --application-version 0 --bootloader-version 1 --bl-settings-version 2 --app-boot-validation VALIDATE_ECDSA_P256_SHA256 --sd-boot-validation VALIDATE_ECDSA_P256_SHA256 --softdevice ${PROJECT_SOURCE_DIR}/${SDK_ROOT}/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --key-file ${PROJECT_SOURCE_DIR}/${PROJ_DIR}/nrfutil_pkg_key.pem ${TARGET_NAME}_settings.hex
                  COMMAND mergehex -m ${BOOTLOADER} ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.hex -o m1.hex
                  COMMAND mergehex -m m1.hex ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_settings.hex -o ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_merged.hex
                  COMMAND rm -f m1.hex
)

add_dependencies(flash ${TARGET_NAME})
add_dependencies(flash_softdev ${TARGET_NAME})
add_dependencies(dfu_app ${TARGET_NAME})
