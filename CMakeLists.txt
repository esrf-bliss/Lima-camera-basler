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
set(NAME "basler")

if(WIN32)
	set(PYLON_ROOT "C:/Program Files/Basler/pylon 5/Development" CACHE PATH "location of pylon sdk")
else()
	set(PYLON_ROOT "/opt/pylon5" CACHE PATH "location of pylon sdk")
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

set(${NAME}_srcs src/BaslerCamera.cpp src/BaslerInterface.cpp src/BaslerDetInfoCtrlObj.cpp
                src/BaslerSyncCtrlObj.cpp src/BaslerRoiCtrlObj.cpp
                src/BaslerBinCtrlObj.cpp src/BaslerVideoCtrlObj.cpp)

add_library(lima${NAME} SHARED
    ${${NAME}_srcs}
)
target_include_directories(lima${NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(lima${NAME} PUBLIC "${PYLON_ROOT}/include")
if ("${PYLON_VERSION}" STREQUAL "3")
  if(WIN32)
    target_include_directories(lima${NAME} PUBLIC "${PYLON_ROOT}/../genicam/library/cpp/include")
  else()
    target_include_directories(lima${NAME} PUBLIC "${PYLON_ROOT}/genicam/library/CPP/include")
  endif()
endif()

target_compile_definitions(lima${NAME} PUBLIC USE_GIGE)
if(WIN32)
    target_compile_definitions(lima${NAME} PRIVATE LIBBASLER_EXPORTS)
endif()

if ("${PYLON_VERSION}" STREQUAL "3")
  if (WIN32)
    set(PYLON_SUFFIX "_MD_VC100" CACHE STRING "Suffix used in Windows for naming Pylon files")
    find_library(BASLER_PYLONBASE_LIB PylonBase${PYLON_SUFFIX} ${PYLON_LIB_DIR})
    find_library(BASLER_PYLONGIGESUPP_LIB PylonGigE${PYLON_SUFFIX}_TL ${PYLON_LIB_DIR})
  else()
    find_library(BASLER_PYLONBASE_LIB pylonbase ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
    find_library(BASLER_PYLONGIGESUPP_LIB pylongigesupp ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
  endif()
  
  if (("${BASLER_PYLONBASE_LIB}" STREQUAL "BASLER_PYLONBASE_LIB-NOTFOUND") OR ("${BASLER_PYLONGIGESUPP_LIB}" STREQUAL "BASLER_PYLONGIGESUPP_LIB-NOTFOUND"))
    message(FATAL_ERROR "${NAME} : pylonbase or pylongige not found")
  endif()
#if("${PYLON_VERSION}" STREQUAL "5")
else()
  if (WIN32)
    set(PYLON_SUFFIX "_MD_VC120" CACHE STRING "Suffix used in Windows for naming Pylon files")
    find_library(BASLER_PYLONBASE_LIB PylonBase${PYLON_SUFFIX}_v5_0 ${PYLON_LIB_DIR})
  else()
    find_library(BASLER_PYLONBASE_LIB pylonbase ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
  endif()

  if (("${BASLER_PYLONBASE_LIB}" STREQUAL "BASLER_PYLONBASE_LIB-NOTFOUND"))
    message(FATAL_ERROR "${NAME} : pylonbase not found")
  endif()  
endif()

if (WIN32)
  target_link_libraries(lima${NAME} ws2_32)
endif()
target_link_libraries(lima${NAME} limacore)
target_link_libraries(lima${NAME} ${BASLER_PYLONBASE_LIB})
if ("${PYLON_VERSION}" STREQUAL "3")
  target_link_libraries(lima${NAME} ${BASLER_PYLONGIGESUPP_LIB})
endif()

if(WIN32)
  if ("${PYLON_VERSION}" STREQUAL "3")
    find_library(BASLER_PYLONGUI_LIB PylonGUI${PYLON_SUFFIX} ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
    find_library(BASLER_PYLONUTILITY_LIB PylonUtility${PYLON_SUFFIX} ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
    find_library(BASLER_PYLONBOOTSTRAPPER_LIB PylonBootstrapper ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
    find_library(BASLER_PYLONGCBASE_LIB GCBase${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(BASLER_PYLONGENAPI_LIB GenApi${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(BASLER_PYLONMATHPARSER_LIB MathParser${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(BASLER_PYLONLOG_LIB Log${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(BASLER_PYLONLOG4CPP_LIB log4cpp${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(BASLER_PYLONLOG4CPPSTATIC_LIB log4cpp-static${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(BASLER_PYLONCLALLSERIAL_LIB CLAllSerial${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(BASLER_PYLONCLPROTOCOL_LIB CLProtocol${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    find_library(BASLER_PYLONCLSERCOM_LIB CLSerCOM ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    target_link_libraries(lima${NAME} ${BASLER_PYLONGUI_LIB} ${BASLER_PYLONUTILITY_LIB}	${BASLER_PYLONBOOTSTRAPPER_LIB} ${BASLER_PYLONGCBASE_LIB} ${BASLER_PYLONGENAPI_LIB} ${BASLER_PYLONMATHPARSER_LIB} ${BASLER_PYLONLOG_LIB} ${BASLER_PYLONLOG4CPP_LIB}	${BASLER_PYLONLOG4CPPSTATIC_LIB} ${BASLER_PYLONCLALLSERIAL_LIB}	${BASLER_PYLONCLPROTOCOL_LIB} ${BASLER_PYLONCLSERCOM_LIB})
  else()
    find_library(BASLER_PYLONGUI_LIB PylonGUI${PYLON_SUFFIX}_v5_0  ${PYLON_LIB_DIR})
    find_library(BASLER_PYLONUTILITY_LIB PylonUtility${PYLON_SUFFIX}_v5_0  ${PYLON_LIB_DIR})
    find_library(BASLER_PYLONGCBASE_LIB GCBase${PYLON_SUFFIX}_v3_0_Basler_pylon_v5_0 ${PYLON_LIB_DIR})   
    find_library(BASLER_PYLONGENAPI_LIB GenApi${PYLON_SUFFIX}_v3_0_Basler_pylon_v5_0 ${PYLON_LIB_DIR})
    target_link_libraries(lima${NAME} ${BASLER_PYLONGUI_LIB} ${BASLER_PYLONUTILITY_LIB} ${BASLER_PYLONGCBASE_LIB} ${BASLER_PYLONGENAPI_LIB})     
  endif()
endif()
  
  
limatools_set_library_soversion(lima${NAME} "VERSION")
if(WIN32)
    set_target_properties(lima${NAME} PROPERTIES PREFIX "lib")
    install(TARGETS lima${NAME}
            DESTINATION lib)
else()
    install(TARGETS lima${NAME}
            LIBRARY DESTINATION lib)
endif()

if(LIMA_ENABLE_PYTHON)
    limatools_run_sip_for_camera(${NAME})
    install(FILES python/__init__.py DESTINATION "${PYTHON_SITE_PACKAGES_DIR}/Lima/Basler")
endif()
