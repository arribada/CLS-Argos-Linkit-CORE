cmake_minimum_required(VERSION 3.13)

project(CLSGenTrackerTests
    VERSION 1.0.0.0
    LANGUAGES CXX C ASM
)

set(ROOT_DIR "..")
set(LINUX_DIR "${ROOT_DIR}/ports/linux")
set(NRF_DIR "${ROOT_DIR}/ports/nrf52840")

# CppUTest
include(FetchContent)
FetchContent_Declare(
    CppUTest
    GIT_REPOSITORY https://github.com/cpputest/cpputest.git
    GIT_TAG        latest-passing-build # or use release tag, eg. v3.8
)
# Set this to ON if you want to have the CppUTests in your project as well.
set(TESTS OFF CACHE BOOL "Switch off CppUTest Test build")
FetchContent_MakeAvailable(CppUTest)
target_compile_definitions(CppUTest PUBLIC CPPUTEST_MEM_LEAK_DETECTION_DISABLED)
target_compile_definitions(CppUTestExt PUBLIC CPPUTEST_MEM_LEAK_DETECTION_DISABLED)

find_package(Threads)
add_executable(${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CMAKE_CXX_STANDARD_REQUIRED ON)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    LFS_NO_DEBUG
    DEBUG_ENABLE
    CPPUTEST
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -g3

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
    -Wno-expansion-to-defined # The NRFX library requires this
    -Wno-maybe-uninitialized
    -Wno-unused-function
    -Wno-format
    
    # Compile prepass library for embedded systems
    -DPREPAS_EMBEDDED
    
    # For enabling debug output
    -DDEBUG_ENABLE
    -DDEBUG_LEVEL=4

    # For setting default device model
    -DDEVICE_MODEL_NAME="SURFACEBOX"s
    -DMODEL_SB=0
    -DMODEL_UW=0
    -DMODEL_CORE=1
    
    # For setting firmware app version
    -DFW_APP_VERSION_STR="V0.1"s
    -DFW_APP_VERSION_STR_C="V0.1"

	# Packet protocol option for including CRC8
    -DARGOS_USE_CRC8
    
    # Workaround allcast encoding bug in CLS messages
    #-DWORKAROUND_ALLCAST_CONSTELLATION_STATUS_ENCODING_BUG
 
    # Always output coloured text from GCC
    -fdiagnostics-color=always
)

# Test related includes
target_include_directories(${PROJECT_NAME} PRIVATE
    "${ROOT_DIR}/libraries/littlefs"
    "${ROOT_DIR}/libraries/prepass/prepass_package_code-PREPAS_V3.4"
    "${ROOT_DIR}/core/hardware"
    "${ROOT_DIR}/core/sm"
    "${ROOT_DIR}/core/ble_services"
    "${ROOT_DIR}/core/configuration"
    "${ROOT_DIR}/core/logging"
    "${ROOT_DIR}/core/filesystem"
    "${ROOT_DIR}/core/scheduling"
    "${ROOT_DIR}/core/services"
    "${ROOT_DIR}/ports/linux"
    "${ROOT_DIR}/core/protocol"
    "${ROOT_DIR}/core/util"
    "${NRF_DIR}/core/hardware"
    "${NRF_DIR}/core/hardware/m8q"
    "mocks"
    "fakes"
)

target_sources(${PROJECT_NAME} PRIVATE
    "${ROOT_DIR}/libraries/littlefs/lfs_util.c"
    "${ROOT_DIR}/libraries/littlefs/lfs.c"
    "${ROOT_DIR}/libraries/prepass/prepass_package_code-PREPAS_V3.4/previpass.c"
    "${ROOT_DIR}/libraries/prepass/prepass_package_code-PREPAS_V3.4/previpass_util.c"
    "${ROOT_DIR}/core/util/haversine.cpp"
    "${ROOT_DIR}/core/scheduling/argos_scheduler.cpp"
    "${ROOT_DIR}/core/services/argos_rx_service.cpp"
    "${ROOT_DIR}/core/services/argos_tx_service.cpp"
    "${ROOT_DIR}/core/services/gps_service.cpp"
    "${ROOT_DIR}/core/services/service.cpp"
    "${ROOT_DIR}/core/filesystem/ota_flash_file_updater.cpp"
    "${ROOT_DIR}/core/services/uwdetector_service.cpp"
    "${ROOT_DIR}/core/hardware/reed.cpp"
    "${ROOT_DIR}/core/hardware/sensor.cpp"
    "${ROOT_DIR}/core/protocol/dte_commands.cpp"
    "${ROOT_DIR}/core/protocol/dte_params.cpp"
    "${ROOT_DIR}/core/sm/gentracker.cpp"
    "${ROOT_DIR}/core/sm/ledsm.cpp"
    "${ROOT_DIR}/core/logging/logger.cpp"
    "${LINUX_DIR}/interrupt_lock.cpp"
    "${NRF_DIR}/core/hardware/nrf_switch.cpp"
    "${NRF_DIR}/core/hardware/nrf_battery_mon.cpp"
    "src/timer_test.cpp"
    "src/filesystem_test.cpp"
    "src/gps_test.cpp"
    "src/scheduler_test.cpp"
    "src/sm_test.cpp"
    "src/encoder_test.cpp"
	"src/decoder_test.cpp"
	"src/config_store_test.cpp"
    "src/dte_handler_test.cpp"
    "src/argos_test.cpp"
    "src/bch_test.cpp"
    "src/crc8_test.cpp"
    "src/crc16_test.cpp"
    "src/crc32_test.cpp"
    "src/previpass_test.cpp"
    "src/sws_test.cpp"
    "src/logger_test.cpp"
    "src/nrf_switch_test.cpp"
    "src/nrf_battery_test.cpp"
    "src/ph_sensor_test.cpp"
    "src/als_sensor_test.cpp"
    "src/sea_temp_sensor_test.cpp"
    "src/cdt_sensor_test.cpp"
    "src/axl_sensor_test.cpp"
    "src/pressure_sensor_test.cpp"
    "src/calibration_test.cpp"
    "src/ubx_ano_test.cpp"
    "src/argos_rx_test.cpp"
    "src/argos_tx_test.cpp"
    "src/main.cpp"
    "fakes/nrfx_rtc.c"
    "mocks/mock_gpio.cpp"
    "mocks/mock_pmu.cpp"
    "mocks/mock_dfu.cpp"
)

set_source_files_properties(${ROOT_DIR}/libraries/prepass/prepass_package_code-PREPAS_V3.4/previpass.c PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-switch-enum -Wno-switch")

target_link_libraries(${PROJECT_NAME} PRIVATE CppUTest CppUTestExt ${CMAKE_THREAD_LIBS_INIT})
