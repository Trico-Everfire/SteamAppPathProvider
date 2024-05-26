cmake_minimum_required(VERSION 3.16)
project(SAPP)
set(CMAKE_CXX_STANDARD 20)

# Options
option(SAPP_BUILD_TESTS "Build tests"                      OFF)
option(SAPP_FETCH_DEPS  "Automatically fetch dependencies"  ON)

# SpeedyKeyV
include(FetchContent)
if (SAPP_FETCH_DEPS AND NOT TARGET keyvalues)
    FetchContent_Declare(
            keyvalues
            GIT_REPOSITORY https://github.com/ozxybox/SpeedyKeyV.git
            GIT_TAG master)
    FetchContent_MakeAvailable(keyvalues)
endif()

# Create library
add_library(${PROJECT_NAME} STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include/SAPP/SAPP.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/SAPP.cpp")

target_link_libraries(${PROJECT_NAME} PRIVATE keyvalues)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Create tests
if(SAPP_BUILD_TESTS)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0)
    FetchContent_MakeAvailable(googletest)
    enable_testing()

    add_executable(${PROJECT_NAME}_test "${CMAKE_CURRENT_SOURCE_DIR}/test/SAPP.cpp")
    target_link_libraries(${PROJECT_NAME}_test PUBLIC ${PROJECT_NAME} gtest_main)

    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME}_test)
endif()
