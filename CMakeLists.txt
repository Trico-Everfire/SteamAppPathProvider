cmake_minimum_required(VERSION 3.0)

project(SAPP)

add_library(SAPP STATIC ${CMAKE_CURRENT_LIST_DIR}/FilesystemSearchProvider.h ${CMAKE_CURRENT_LIST_DIR}/FilesystemSearchProvider.cpp)
target_include_directories(SAPP PUBLIC ${CMAKE_CURRENT_LIST_DIR})
