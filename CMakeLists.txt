cmake_minimum_required(VERSION 3.15)
project(SAPP)
set(CMAKE_CXX_STANDARD 23)

include(GNUInstallDirs)

add_library(${PROJECT_NAME} STATIC "${CMAKE_CURRENT_LIST_DIR}/include/sapp/SteamAppPathProvider.h")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include/")

option(SAPP_BUILD_TESTS "Build tests for SAPP" OFF)

option(SAPP_INCLUDE_SPEEDY "fetch content of SpeedyKeyV" ON)

if(SAPP_BUILD_TESTS OR SAPP_BUILD_TESTS)
    include(FetchContent)
    # SpeedyKeyV
    FetchContent_Declare(
            keyvalues
            GIT_REPOSITORY https://github.com/ozxybox/SpeedyKeyV.git
            GIT_TAG master)
    FetchContent_MakeAvailable(keyvalues)
endif ()

if(SAPP_BUILD_TESTS)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
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
