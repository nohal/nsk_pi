# ~~~
# Summary:      Tests build script
# Copyright (c) 2022 Pavel Kalian
# License:      GPLv3+
# ~~~

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

cmake_minimum_required( VERSION 3.0 )

project( NSKTests CXX )

include(FetchContent)

FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.2.0)

FetchContent_MakeAvailable(Catch2)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${Catch2_SOURCE_DIR}/extras")

set( SOURCES_TESTS
        ${SRC_N} )

include_directories("${CMAKE_SOURCE_DIR}/include")

add_executable(tests ${SOURCES_TESTS})
    target_link_libraries(tests ${wxWidgets_LIBRARIES})
endif()
target_link_libraries(tests Catch2::Catch2WithMain)

add_subdirectory("opencpn-libs/${PKG_API_LIB}")
target_link_libraries(tests ocpn::api)

include(CTest)
include(Catch)
catch_discover_tests(tests)
