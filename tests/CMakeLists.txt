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


add_executable(${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

# Test related includes
target_include_directories(${PROJECT_NAME} PRIVATE
    "${ROOT_DIR}/libraries/littlefs"
    "${ROOT_DIR}/core/filesystem"
)

target_sources(${PROJECT_NAME} PRIVATE
    "${ROOT_DIR}/libraries/littlefs/lfs_util.c"
    "${ROOT_DIR}/libraries/littlefs/lfs.c"
    "src/filesystem_test.cpp"
    "src/main.cpp"
)

target_link_libraries(${PROJECT_NAME} PRIVATE CppUTest CppUTestExt)
