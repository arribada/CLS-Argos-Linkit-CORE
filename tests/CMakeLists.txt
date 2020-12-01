cmake_minimum_required(VERSION 3.13)

project(CLSGenTrackerTests
    VERSION 1.0.0.0
    LANGUAGES CXX C ASM
)

set(ROOT_DIR "..")

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

find_package(Threads)
add_executable(${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-g")

# Test related includes
target_include_directories(${PROJECT_NAME} PRIVATE
    "${ROOT_DIR}/libraries/littlefs"
    "${ROOT_DIR}/core/hardware"
    "${ROOT_DIR}/core/sm"
    "${ROOT_DIR}/core/ble_services"
    "${ROOT_DIR}/core/configuration"
    "${ROOT_DIR}/core/logging"
    "${ROOT_DIR}/core/filesystem"
    "${ROOT_DIR}/core/scheduling"
    "${ROOT_DIR}/ports/linux"
    "${ROOT_DIR}/core/protocol"
    "mocks"
    "fakes"
)

target_sources(${PROJECT_NAME} PRIVATE
    "${ROOT_DIR}/libraries/littlefs/lfs_util.c"
    "${ROOT_DIR}/libraries/littlefs/lfs.c"
    "src/timer_test.cpp"
    "src/filesystem_test.cpp"
    "src/scheduler_test.cpp"
    "src/sm_test.cpp"
    "src/encoder_test.cpp"
	"src/decoder_test.cpp"
     "src/main.cpp"
)

target_link_libraries(${PROJECT_NAME} PRIVATE CppUTest CppUTestExt ${CMAKE_THREAD_LIBS_INIT})
