
SET(PYLON_ROOT "/opt/pylon" CACHE PATH "location of pylon sdk")

SET(CMAKE_SHARED_LINKER_FLAGS "-Wl,--add-needed")
LINK_DIRECTORIES(${PYLON_ROOT}/genicam/bin/Linux64_x64)

SET(basler_srcs src/BaslerCamera.cpp src/BaslerInterface.cpp src/BaslerDetInfoCtrlObj.cpp
                src/BaslerSyncCtrlObj.cpp src/BaslerRoiCtrlObj.cpp
                src/BaslerBinCtrlObj.cpp src/BaslerVideoCtrlObj.cpp)

FILE(STRINGS "VERSION" basler_ver)
ADD_LIBRARY(limabasler SHARED
    ${basler_srcs}
)
TARGET_INCLUDE_DIRECTORIES(limabasler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
TARGET_INCLUDE_DIRECTORIES(limabasler PUBLIC "${PYLON_ROOT}/include")
if(WIN32)
    TARGET_INCLUDE_DIRECTORIES(limabasler PUBLIC "${PYLON_ROOT}/../genicam/library/cpp/include")
ELSE()
    TARGET_INCLUDE_DIRECTORIES(limabasler PUBLIC "${PYLON_ROOT}/genicam/library/CPP/include")
ENDIF()
TARGET_COMPILE_DEFINITIONS(limabasler PUBLIC USE_GIGE)
IF(WIN32)
    TARGET_COMPILE_DEFINITIONS(limabasler PRIVATE LIBBASLER_EXPORTS)
ENDIF()

IF (WIN32)
    SET(PYLON_SUFFIX "_MD_VC100" CACHE STRING "Suffix used in Windows for naming Pylon files")
    FIND_LIBRARY(PYLONBASE PylonBase${PYLON_SUFFIX} ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
    FIND_LIBRARY(PYLONGIGESUPP PylonGigE${PYLON_SUFFIX}_TL ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
ELSE()
    FIND_LIBRARY(PYLONBASE pylonbase ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
    FIND_LIBRARY(PYLONGIGESUPP pylongigesupp ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
ENDIF()
IF (("${PYLONBASE}" STREQUAL "PYLONBASE-NOTFOUND") OR
    ("${PYLONGIGESUPP}" STREQUAL "PYLONGIGESUPP-NOTFOUND"))
    MESSAGE(FATAL_ERROR "pylonbase or pylongigesupp not found")
ENDIF()

TARGET_LINK_LIBRARIES(limabasler limacore)
TARGET_LINK_LIBRARIES(limabasler ${PYLONBASE})
TARGET_LINK_LIBRARIES(limabasler ${PYLONGIGESUPP})
IF(WIN32)
    FIND_LIBRARY(PYLONGUI PylonGUI${PYLON_SUFFIX} ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
    FIND_LIBRARY(PYLONUTILITY PylonUtility${PYLON_SUFFIX} ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
    FIND_LIBRARY(PYLONBOOTSTRAPPER PylonBootstrapper ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
    FIND_LIBRARY(PYLONGCBASE GCBase${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    FIND_LIBRARY(PYLONGENAPI GenApi${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    FIND_LIBRARY(PYLONMATHPARSER MathParser${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    FIND_LIBRARY(PYLONLOG Log${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    FIND_LIBRARY(PYLONLOG4CPP log4cpp${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    FIND_LIBRARY(PYLONLOG4CPPSTATIC log4cpp-static${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    FIND_LIBRARY(PYLONCLALLSERIAL CLAllSerial${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    FIND_LIBRARY(PYLONCLPROTOCOL CLProtocol${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    FIND_LIBRARY(PYLONCLSERCOM CLSerCOM ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
    TARGET_LINK_LIBRARIES(limabasler ${PYLONGUI} ${PYLONUTILITY} ${PYLONBOOTSTRAPPER} ${PYLONGCBASE} ${PYLONGENAPI} ${PYLONMATHPARSER} ${PYLONLOG} ${PYLONLOG4CPP} ${PYLONLOG4CPPSTATIC} ${PYLONCLALLSERIAL} ${PYLONCLPROTOCOL} ${PYLONCLSERCOM})
ENDIF()
SET_TARGET_PROPERTIES(limabasler PROPERTIES VERSION ${basler_ver} SOVERSION "1")
if(WIN32)
    INSTALL(TARGETS limabasler
            DESTINATION lib)
else()
    INSTALL(TARGETS limabasler
            LIBRARY DESTINATION lib)
endif()

IF(COMPILE_SIP)
    SET(NAME "basler")
    SET(INCLUDES)
    FILE(GLOB sipfiles
         RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/sip"
         "${CMAKE_CURRENT_SOURCE_DIR}/sip/*.sip")
    FOREACH(sipfile ${sipfiles})
        SET(INCLUDES
"${INCLUDES}
%Include ${sipfile}")
    ENDFOREACH()

    CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/sip/limamodules.sip.in sip/limabasler.sip)
    SET(SIP_CONCAT_PARTS 1)
    SET(SIP_INCLUDES ${SIP_INCLUDES}
        "${CMAKE_SOURCE_DIR}/third-party/Processlib/sip"
        "${CMAKE_BINARY_DIR}/sip/core"
        "${CMAKE_SOURCE_DIR}/third-party/Processlib/tasks/sip"
        "${CMAKE_SOURCE_DIR}/common/sip"
        "${CMAKE_SOURCE_DIR}/hardware/sip"
        "${CMAKE_SOURCE_DIR}/control/sip"
        "${CMAKE_SOURCE_DIR}/control/software_operation/sip"
        "${CMAKE_CURRENT_SOURCE_DIR}/sip")
    set(SIP_DISABLE_FEATURES WITH_CONFIG)
    set(SIP_CHECK_EXC ON)
    ADD_SIP_PYTHON_MODULE(limabasler ${CMAKE_CURRENT_BINARY_DIR}/sip/limabasler.sip)
    TARGET_INCLUDE_DIRECTORIES(python_module_limabasler PRIVATE
                               ${PYTHON_INCLUDE_DIRS}
                               "${CMAKE_SOURCE_DIR}/sip"
                               "${CMAKE_SOURCE_DIR}/sip/core")
    TARGET_LINK_LIBRARIES(python_module_limabasler limabasler)

    INSTALL(FILES python/__init__.py DESTINATION "${PYTHON_SITE_PACKAGES_DIR}/Lima/Basler")
ENDIF()
