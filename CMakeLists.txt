cmake_minimum_required(VERSION 3.15)
project(SAPP)
set(CMAKE_CXX_STANDARD 23)

include(GNUInstallDirs)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/sapp/SteamAppPathProvider.h>")
target_include_directories(${PROJECT_NAME} INTERFACE "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>")
target_link_libraries(${PROJECT_NAME} INTERFACE keyvalues)

option(SAPP_BUILD_TESTS "Build tests for SAPP" OFF)
if(SAPP_BUILD_TESTS)
    include(FetchContent)

    # SpeedyKeyV
    FetchContent_Declare(
            keyvalues
            GIT_REPOSITORY https://github.com/ozxybox/SpeedyKeyV.git
            GIT_TAG master)
    FetchContent_MakeAvailable(keyvalues)

    # Google Test
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG release-1.12.1)
    FetchContent_MakeAvailable(googletest)
    enable_testing()

    add_executable(${PROJECT_NAME}_test ${CMAKE_CURRENT_SOURCE_DIR}/test/SAPPTests.cpp)
    target_link_libraries(${PROJECT_NAME}_test PUBLIC ${PROJECT_NAME} keyvalues gtest_main)
    target_include_directories(
            ${PROJECT_NAME}_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include)

    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME}_test)
endif()
