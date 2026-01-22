###########################################################################
# This file is part of LImA, a Library for Image Acquisition
#
#  Copyright (C) : 2009-2026
#  European Synchrotron Radiation Facility
#  CS40220 38043 Grenoble Cedex 9
#  FRANCE
#
#  Contact: lima@esrf.fr
#
#  This is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  This software is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, see <http://www.gnu.org/licenses/>.
############################################################################

cmake_minimum_required(VERSION 4.0) 

project(basler)

# Include additional modules that are used inconditionnaly
include(GNUInstallDirs)
include(GenerateExportHeader)

# If conda build, always set lib dir to 'lib'
if($ENV{CONDA_BUILD})
  set(CMAKE_INSTALL_LIBDIR "lib")
endif()

# Set lower / upper case project names
string(TOUPPER "${PROJECT_NAME}" PROJECT_NAME_UPPER)
string(TOLOWER "${PROJECT_NAME}" PROJECT_NAME_LOWER)

# Set targets export name (used by lima and dependencies)
set(TARGETS_EXPORT_NAME "${PROJECT_NAME_LOWER}-targets")
#set(namespace "${PROJECT_NAME}::")

# Check if project is being used directly or via add_subdirectory
set(CAMERA_MASTER_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(CAMERA_MASTER_PROJECT ON)
endif()

if (CAMERA_MASTER_PROJECT)
  find_package(Lima REQUIRED COMPONENTS devel tools)
endif()

# CMake additional macros
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${LIMA_CMAKE_INCLUDE_DIRS} ${CMAKE_MODULE_PATH})

include(LimaTools)

# Set version
include(project_version)

# Enable python binding code compilation using sip generator
if (CAMERA_MASTER_PROJECT)
  option(LIMA_ENABLE_PYTHON "compile python binding code?" OFF)
  option(LIMA_ENABLE_PYTANGO_SERVER "install python tango server?" OFF)

  if (LIMA_ENABLE_PYTHON)
    limatools_find_python_and_sip()
  endif()
endif()

set(PYLON_VERSION "6" CACHE STRING "Pylon version 6")

if ("${PYLON_VERSION}" STREQUAL "6")
  find_package(Pylon6 REQUIRED)
endif()

file(GLOB_RECURSE BASLER_INCS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

# Library definition
add_library(basler SHARED
  src/BaslerCamera.cpp
  src/BaslerInterface.cpp
  src/BaslerDetInfoCtrlObj.cpp
  src/BaslerSyncCtrlObj.cpp
  src/BaslerRoiCtrlObj.cpp
  src/BaslerBinCtrlObj.cpp
  src/BaslerVideoCtrlObj.cpp
  ${BASLER_INCS}
)

# Generate export macros
generate_export_header(basler)

# Set version and output name
set_target_properties(basler PROPERTIES
  OUTPUT_NAME "lima${PROJECT_NAME_LOWER}"
  VERSION "${PROJECT_VERSION}"
  SOVERSION "${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}")

# Add dependencies
target_include_directories(basler
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"           # For export header
    PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(basler PUBLIC limacore)

target_compile_definitions(basler PUBLIC ${PYLON_DEFINITIONS})

target_include_directories(basler PUBLIC ${PYLON_INCLUDE_DIRS})

target_link_libraries(basler PUBLIC ${PYLON_LIBRARIES})

if(WIN32)
  target_compile_definitions(basler
    PRIVATE basler_EXPORTS
    PUBLIC NOMINMAX)
  set_target_properties(basler PROPERTIES PREFIX "lib" IMPORT_PREFIX "lib")
endif()

# Binding code for python
if(LIMA_ENABLE_PYTHON)
  limatools_run_sip_for_camera(basler 4)
endif()

# Generate and install package config file and version
if(CAMERA_MASTER_PROJECT)
    set (PROJECT_LIBRARIES basler)
    set(SIP_INSTALL_DIR ${CMAKE_INSTALL_DATADIR}/sip/lima)
    set(CMAKE_INSTALL_DIR ${CMAKE_INSTALL_DATADIR}/cmake/lima)
    include(package_config)
endif()

## Installation

install(
  TARGETS basler
  EXPORT "${TARGETS_EXPORT_NAME}"
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}   # import library
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}   # .so files are libraries
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}   # .dll files are binaries
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}  # this does not actually install anything (but used by downstream projects)
)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  COMPONENT devel
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/project_version.h
    ${PROJECT_BINARY_DIR}/basler_export.h
  COMPONENT devel
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(LIMA_ENABLE_PYTHON)
  install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python/
    DESTINATION "$<PATH:CMAKE_PATH,NORMALIZE,${Python3_SITEARCH}/lima/basler>"
    )
  if (LIMA_ENABLE_PYTANGO_SERVER)
    add_subdirectory(tango)
  endif()
endif()

## Tests
if(CAMERA_ENABLE_TESTS)
    enable_testing()
    #add_subdirectory(test)
endif()
