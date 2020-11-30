cmake_minimum_required(VERSION 3.1)

project(GenTracker
    VERSION 1.0.0.0
    LANGUAGES CXX C ASM
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDK_ROOT      drivers/nRF5_SDK_17.0.2)
set(PROJ_DIR      .)
set(CORE_DIR      ../../core)
set(LIB_DIR       ../../libraries)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    ${PROJ_DIR}/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJ_DIR}
    ${LIB_DIR}/littlefs
    ${CORE_DIR}/ble_services
    ${CORE_DIR}/configuration
    ${CORE_DIR}/filesystem
    ${CORE_DIR}/logging
    ${CORE_DIR}/scheduling
    ${CORE_DIR}/sm
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
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

    # Build flags
    -mcpu=cortex-m4
    -mthumb
    -mabi=aapcs
    -ffunction-sections
    -fdata-sections
    -fno-strict-aliasing
    -fno-builtin
    -fshort-enums

    # Always output coloured text from GCC
    -fdiagnostics-color=always
)

target_link_options(${PROJECT_NAME} PRIVATE
    -T${PROJECT_SOURCE_DIR}/gcc_nrf52840.ld
    -mthumb -mabi=aapcs
    -Wl,--gc-sections
    --specs=nano.specs
    -u _printf_float
    -u _scanf_float
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
                  COMMENT "Flashing the bootloader"
                  COMMAND nrfjprog -f nrf52 --eraseall
                  COMMAND nrfjprog -f nrf52 --program ${PROJECT_SOURCE_DIR}/${SDK_ROOT}/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --sectoranduicrerase
                  COMMAND nrfjprog -f nrf52 --program ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex --sectorerase
                  COMMAND nrfjprog -f nrf52 --reset
)
add_dependencies(flash ${PROJECT_NAME})