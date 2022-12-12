# ~~~
# Summary:      Local, non-generic plugin setup
# Copyright (c) 2020-2021 Mike Rossiter
# Copyright (c) 2022 Pavel Kalian
# License:      GPLv3+
# ~~~

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.


# -------- Options ----------

set(OCPN_TEST_REPO
  "nohal/opencpn-plugins"
  CACHE STRING "Default repository for untagged builds"
)
set(OCPN_BETA_REPO
  "nohal/nsk_pi-beta"
  CACHE STRING
  "Default repository for tagged builds matching 'beta'"
)
set(OCPN_RELEASE_REPO
  "nohal/nsk_pi-stable"
  CACHE STRING
  "Default repository for tagged builds not matching 'beta'"
)

#
#
# -------  Plugin setup --------
#
set(PKG_NAME NSK_pi)
set(PKG_VERSION "0.0.1")
set(PKG_PRERELEASE "beta")  # Empty, or a tag like 'beta'

set(DISPLAY_NAME NSK)    # Dialogs, installer artifacts, ...
set(PLUGIN_API_NAME nsk_pi) # As of GetCommonName() in plugin API
set(PKG_SUMMARY "NMEA0183 to SignalK converter plugin for OpenCPN")
set(PKG_DESCRIPTION [=[
  NMEA0183 to SignalK converter plugin for OpenCPN
]=])

set(PKG_AUTHOR "Pavel Kalian")
set(PKG_IS_OPEN_SOURCE "yes")
set(PKG_HOMEPAGE https://github.com/nohal/nsk_pi)
set(PKG_INFO_URL https://opencpn.org/OpenCPN/plugins/nsk.html)

option(WITH_TESTS "Whether or not to build the tests" OFF)
option(SANITIZE "What sanitizers to use" "")

if(NOT "${SANITIZE}" STREQUAL "OFF" AND NOT "${SANITIZE}" STREQUAL "")
  add_compile_options(-fsanitize=${SANITIZE} -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${SANITIZE} -fno-omit-frame-pointer)
endif()

add_definitions(-DNSK_USE_SVG)
add_definitions(-DocpnUSE_GL)
add_definitions(-DRAPIDJSON_HAS_STDSTRING=1)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(HDR_N
  #${CMAKE_SOURCE_DIR}/include/nsk.h
)
set(SRC_N
  #${CMAKE_SOURCE_DIR}/src/nsk.cpp
)

set(SRC
  ${HDR_N}
  ${SRC_N}
  ${CMAKE_SOURCE_DIR}/include/nsk_pi.h
  ${CMAKE_SOURCE_DIR}/src/nsk_pi.cpp
)

set(PKG_API_LIB api-18)  #  A dir in opencpn-libs/ e. g., api-17 or api-16

macro(late_init)
  # Perform initialization after the PACKAGE_NAME library, compilers
  # and ocpn::api is available.

  # Prepare doxygen config
  configure_file(${CMAKE_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile)
  configure_file(${CMAKE_SOURCE_DIR}/doc/header.html.in ${CMAKE_BINARY_DIR}/header.html)
  # Prepare asciidoxy
  configure_file(${CMAKE_SOURCE_DIR}/doc/api.adoc.in ${CMAKE_BINARY_DIR}/api.adoc @ONLY)
  configure_file(${CMAKE_SOURCE_DIR}/doc/packages.toml ${CMAKE_BINARY_DIR}/packages.toml)
  configure_file(${CMAKE_SOURCE_DIR}/doc/contents.toml ${CMAKE_BINARY_DIR}/contents.toml)
endmacro()

macro(add_plugin_libraries)
  add_subdirectory("${CMAKE_SOURCE_DIR}/buildwin/marnav")
  target_link_libraries(${PACKAGE_NAME} marnav::marnav)
  if(NOT WIN32)
    target_link_libraries(${PACKAGE_NAME} marnav::marnav-io)
  endif()
  include_directories(${MARNAV_INCLUDE_DIRS})
endmacro()

if(${WITH_TESTS})
  include(CTest)
  add_subdirectory("${CMAKE_SOURCE_DIR}/tests")
  add_dependencies(${CMAKE_PROJECT_NAME} tests)
endif()

add_custom_target(doxygen-docs
  COMMAND doxygen
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(asciidoxy-docs
  COMMAND asciidoxy -D apidocs --spec-file packages.toml api.adoc
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  DEPENDS doxygen-docs
)
