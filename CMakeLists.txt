###########################################################################
# This file is part of LImA, a Library for Image Acquisition
#
#  Copyright (C) : 2009-2017
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
if(WIN32)
	set(PYLON_ROOT "C:/Program Files/Basler/pylon 5/Development" CACHE PATH "location of pylon sdk")
else()
	set(PYLON_ROOT "/opt/pylon" CACHE PATH "location of pylon sdk")
endif()

set(PYLON_VERSION "5" CACHE STRING "Pylon version 3 or 5")
if(UNIX)
  set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--add-needed")
endif()

# Pylon SDK installer installs both 32 and 64 libs, so give find_library() single path 
if(WIN32)
  message ("Win arch is  " ${CMAKE_SYSTEM_PROCESSOR})
  if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(PYLON_LIB_DIR ${PYLON_ROOT}/lib/x64)
  else()
    set (PYLON_LIB_DIR ${PYLON_ROOT}/lib/win32)
  endif()
endif()

link_directories(${PYLON_ROOT}/genicam/bin/Linux64_x64)

set(basler_srcs src/BaslerCamera.cpp src/BaslerInterface.cpp src/BaslerDetInfoCtrlObj.cpp
                src/BaslerSyncCtrlObj.cpp src/BaslerRoiCtrlObj.cpp
                src/BaslerBinCtrlObj.cpp src/BaslerVideoCtrlObj.cpp)

file(STRINGS "VERSION" basler_ver)
add_library(limabasler SHARED
    ${basler_srcs}
)
target_include_directories(limabasler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(limabasler PUBLIC "${PYLON_ROOT}/include")
if ("${PYLON_VERSION}" STREQUAL "3")
  if(WIN32)
    target_include_directories(limabasler PUBLIC "${PYLON_ROOT}/../genicam/library/cpp/include")
  else()
    target_include_directories(limabasler PUBLIC "${PYLON_ROOT}/genicam/library/CPP/include")
  endif()
endif()

target_compile_definitions(limabasler PUBLIC USE_GIGE)
if(WIN32)
    target_compile_definitions(limabasler PRIVATE LIBBASLER_EXPORTS)
endif()

if ("${PYLON_VERSION}" STREQUAL "3")
  if (WIN32)
    set(PYLON_SUFFIX "_MD_VC100" CACHE STRING "Suffix used in Windows for naming Pylon files")
    find_library(PYLONBASE PylonBase${PYLON_SUFFIX} ${PYLON_LIB_DIR})
    find_library(PYLONGIGESUPP PylonGigE${PYLON_SUFFIX}_TL ${PYLON_LIB_DIR})
  else()
    find_library(PYLONBASE pylonbase ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
    find_library(PYLONGIGESUPP pylongigesupp ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
  endif()
  
  if (("${PYLONBASE}" STREQUAL "PYLONBASE-NOTFOUND") OR ("${PYLONGIGESUPP}" STREQUAL "PYLONGIGESUPP-NOTFOUND"))
    message(FATAL_ERROR "pylonbase or pylongige not found")
  endif()
#if("${PYLON_VERSION}" STREQUAL "5")
else()
  if (WIN32)
    set(PYLON_SUFFIX "_MD_VC120" CACHE STRING "Suffix used in Windows for naming Pylon files")
    find_library(PYLONBASE PylonBase${PYLON_SUFFIX}_v5_0 ${PYLON_LIB_DIR})
  else()
    find_library(PYLONBASE pylonbase ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
  endif()

  if (("${PYLONBASE}" STREQUAL "PYLONBASE-NOTFOUND"))
    message(FATAL_ERROR "pylonbase not found")
  endif()  
endif()

if (WIN32)
  target_link_libraries(limabasler ws2_32)
endif()
target_link_libraries(limabasler limacore)
target_link_libraries(limabasler ${PYLONBASE})
if ("${PYLON_VERSION}" STREQUAL "3")
  target_link_libraries(limabasler ${PYLONGIGESUPP})
endif()

if(WIN32)
  if ("${PYLON_VERSION}" STREQUAL "3")
    find_library(PYLONGUI PylonGUI${PYLON_SUFFIX} ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
    find_library(PYLONUTILITY PylonUtility${PYLON_SUFFIX} ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
    find_library(PYLONBOOTSTRAPPER PylonBootstrapper ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
    find_library(PYLONGCBASE GCBase${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(PYLONGENAPI GenApi${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(PYLONMATHPARSER MathParser${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(PYLONLOG Log${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(PYLONLOG4CPP log4cpp${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(PYLONLOG4CPPSTATIC log4cpp-static${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(PYLONCLALLSERIAL CLAllSerial${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(PYLONCLPROTOCOL CLProtocol${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(PYLONCLSERCOM CLSerCOM ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    target_link_libraries(limabasler ${PYLONGUI} ${PYLONUTILITY} ${PYLONBOOTSTRAPPER} ${PYLONGCBASE} ${PYLONGENAPI} ${PYLONMATHPARSER} ${PYLONLOG} ${PYLONLOG4CPP} ${PYLONLOG4CPPSTATIC} ${PYLONCLALLSERIAL} ${PYLONCLPROTOCOL} ${PYLONCLSERCOM})
  else()
    find_library(PYLONGUI PylonGUI${PYLON_SUFFIX}_v5_0  ${PYLON_LIB_DIR})
    find_library(PYLONUTILITY PylonUtility${PYLON_SUFFIX}_v5_0  ${PYLON_LIB_DIR})
    find_library(PYLONGCBASE GCBase${PYLON_SUFFIX}_v3_0_Basler_pylon_v5_0 ${PYLON_LIB_DIR})   
    find_library(PYLONGENAPI GenApi${PYLON_SUFFIX}_v3_0_Basler_pylon_v5_0 ${PYLON_LIB_DIR})
    target_link_libraries(limabasler ${PYLONGUI} ${PYLONUTILITY} ${PYLONGCBASE} ${PYLONGENAPI})     
  endif()
endif()
  
  
set_target_properties(limabasler PROPERTIES VERSION ${basler_ver} SOVERSION "1")

if(WIN32)
    set_target_properties(limabasler PROPERTIES PREFIX "lib")
endif()

if(WIN32)
    install(TARGETS limabasler
            DESTINATION lib)
else()
    install(TARGETS limabasler
            LIBRARY DESTINATION lib)
endif()

if(COMPILE_SIP)
    set(NAME "basler")
    include(CameraRunSIP)
    install(FILES python/__init__.py DESTINATION "${PYTHON_SITE_PACKAGES_DIR}/Lima/Basler")
endif()
