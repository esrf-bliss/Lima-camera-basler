
SET(PYLON_ROOT "/opt/pylon" CACHE STRING "location of pylon sdk")

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
TARGET_INCLUDE_DIRECTORIES(limabasler PUBLIC "${PYLON_ROOT}/genicam/library/CPP/include")
TARGET_COMPILE_DEFINITIONS(limabasler PUBLIC USE_GIGE)

FIND_LIBRARY(PYLONBASE pylonbase ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
FIND_LIBRARY(PYLONGIGESUPP pylongigesupp ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
IF (("${PYLONBASE}" STREQUAL "PYLONBASE-NOTFOUND") OR
    ("${PYLONGIGESUPP}" STREQUAL "PYLONGIGESUPP-NOTFOUND"))
    MESSAGE(FATAL_ERROR "pylonbase or pylongigesupp not found")
ENDIF()

TARGET_LINK_LIBRARIES(limabasler limacore)
TARGET_LINK_LIBRARIES(limabasler ${PYLONBASE})
TARGET_LINK_LIBRARIES(limabasler ${PYLONGIGESUPP})
SET_TARGET_PROPERTIES(limabasler PROPERTIES VERSION ${basler_ver} SOVERSION "1")
INSTALL(TARGETS limabasler
        LIBRARY DESTINATION lib)

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
