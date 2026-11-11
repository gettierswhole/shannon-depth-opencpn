# ~~~
# Summary:      Local, non-generic plugin setup
# Copyright (c) 2020-2021 Mike Rossiter (template)
# License:      GPLv3+
# ~~~

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.


# -------- Options ----------

set(OCPN_TEST_REPO
    "gettierswhole/shannondepth-alpha"
    CACHE STRING "Default repository for untagged builds"
)
set(OCPN_BETA_REPO
    "gettierswhole/shannondepth-beta"
    CACHE STRING
    "Default repository for tagged builds matching 'beta'"
)
set(OCPN_RELEASE_REPO
    "gettierswhole/shannondepth-prod"
    CACHE STRING
    "Default repository for tagged builds not matching 'beta'"
)

#
#
# -------  Plugin setup --------
#
set(PKG_NAME ShannonDepth_pi)
set(PKG_VERSION 0.1.0) # Major.Minor.Patch
set(PKG_PRERELEASE "")  # Empty, or a tag like 'beta'

set(DISPLAY_NAME "Shannon Depth")    # Dialogs, installer artifacts, ...
set(PLUGIN_API_NAME ShannonDepth) # As of GetCommonName() in plugin API
set(PKG_SUMMARY "Today's Shannon water level against CSIG chart datum")
set(PKG_DESCRIPTION [=[
Shows how far today's water level is above or below the CSIG chart datum
for the stretch of the Shannon the boat is in, using the live gauges and
datums of the Shannon Chart Depth page. Untested experiment, not an aid
to navigation.
]=])

set(PKG_AUTHOR "Shannon Chart Depth")
set(PKG_IS_OPEN_SOURCE "yes")
set(PKG_HOMEPAGE https://github.com/gettierswhole/shannon-depth-opencpn)
set(PKG_INFO_URL https://github.com/gettierswhole/shannon-depth-opencpn)

set(SRC
    src/shannondepth_pi.h
    src/shannondepth_pi.cpp
    src/plug_utils.cpp
    src/plug_utils.h
    src/core/depth_core.h
    src/core/depth_core.cpp
)

set(PKG_API_LIB api-18)  #  A dir in opencpn-libs/ e. g., api-17 or api-16

# Minimum version which should be loaded. Ignored if api < 22.
# Allows plugins using functions from 1.22+ API to be loaded by
# hosts only supporting 1.21; plugins then needs to check if host
# supports 1.22+ before using such functions.
## set(MIN_API_VERSION 1.21)

macro(late_init)
  # Perform initialization after the PACKAGE_NAME library, compilers
  # and ocpn::api is available.
  if (APPLE)
    target_compile_definitions(${PACKAGE_NAME} PUBLIC OCPN_GHC_FILESYSTEM)
  endif ()
endmacro ()

macro(add_plugin_libraries)
  # Add libraries required by this plugin
  add_subdirectory("${CMAKE_SOURCE_DIR}/libs/std_filesystem")
  target_link_libraries(${PACKAGE_NAME} ocpn::filesystem)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/tinyxml")
  target_link_libraries(${PACKAGE_NAME} ocpn::tinyxml)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxJSON")
  target_link_libraries(${PACKAGE_NAME} ocpn::wxjson)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/plugin_dc")
  target_link_libraries(${PACKAGE_NAME} ocpn::plugin-dc)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/jsoncpp")
  target_link_libraries(${PACKAGE_NAME} ocpn::jsoncpp)

  # The wxsvg library enables SVG overall in the plugin
  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxsvg")
  target_link_libraries(${PACKAGE_NAME} ocpn::wxsvg)
endmacro ()
