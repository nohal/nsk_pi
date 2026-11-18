# ~~~
# Summary:      Tests build script
# Copyright (c) 2022 Pavel Kalian
# License:      GPLv3+
# ~~~

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 3 of the License, or (at your option) any later
# version.

cmake_minimum_required(VERSION 3.0)

project(NSKTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.2.0)

FetchContent_MakeAvailable(Catch2)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(Catch2 PRIVATE -include cstdint)
endif()
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${Catch2_SOURCE_DIR}/extras")

set(SOURCES_TESTS
    opencpn_mock.h
    opencpn_mock.cpp
    utils.h
    001-gll.cpp
    002-extended-sentences.cpp
    003-n2k.cpp
    ${SRC_N})

include_directories("${CMAKE_SOURCE_DIR}/include" ${wxWidgets_INCLUDE_DIR})

add_executable(tests ${SOURCES_TESTS})
target_link_libraries(tests ${wxWidgets_LIBRARIES})
target_link_libraries(tests Catch2::Catch2WithMain)

add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/n2kparser"
                 "${CMAKE_CURRENT_BINARY_DIR}/n2kparser")
target_link_libraries(tests ocpn::N2KParser)

add_subdirectory("opencpn-libs/${PKG_API_LIB}")
target_link_libraries(tests ocpn::api)

target_link_libraries(tests marnav::marnav)
include_directories(${MARNAV_INCLUDE_DIRS})

include(CTest)
include(Catch)
catch_discover_tests(tests)
