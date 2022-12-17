# Wrapper for marnav
#
# Exports: marnav::marnav transitive link object
#
# Marnav requires to be built with clang compiler and on Windows
# needs patching
#
# Copyright (C) 2022 Pavel Kalian
#
# This program is free software; you can redistribute it and/or modify  *
# it under the terms of the GNU General Public License as published by  *
# the Free Software Foundation; either version 3 of the License, or     *
# (at your option) any later version.                                   *

cmake_minimum_required(VERSION 3.1.0)

if(TARGET marnav::marnav)
  return()
endif()

if(QT_ANDROID)
  message(STATUS "NOTE: marnav is not available on Android")
  return()
endif()

project(marnav_marnav)

if(WIN32)
  add_library(marnav_if INTERFACE)
  add_library(marnav::marnav ALIAS marnav_if)
  set(MARNAV_LIBRARY ${CMAKE_SOURCE_DIR}/buildwin/marnav/win32/lib/marnav.lib PARENT_SCOPE)
  set(MARNAV_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/marnav/include
        CACHE STRING "Windows marnav headers directory" PARENT_SCOPE
  )
  target_link_libraries(marnav_if INTERFACE ${MARNAV_LIBRARY})
  target_include_directories(marnav_if INTERFACE  ${MARNAV_INCLUDE_DIRS})
else()
  #include(FetchContent)

  #FetchContent_Declare(
  #  marnav
  #  GIT_REPOSITORY https://github.com/mariokonrad/marnav.git
  #  GIT_TAG master
  #)
  set(ENABLE_STATIC ON)
  set(BUILD_SHARED_LIBS FALSE)
  set(ENABLE_EXAMPLES OFF)
  set(ENABLE_TESTS OFF)
  set(ENABLE_TESTS_BENCHMARK OFF)
  set(ENABLE_TOOLS OFF)
  #FetchContent_MakeAvailable(marnav)

  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/marnav)

  set(MARNAV_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/marnav/include" PARENT_SCOPE)
endif()
