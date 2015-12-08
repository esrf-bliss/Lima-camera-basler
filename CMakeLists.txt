
set(PYLON_ROOT "/opt/pylon" CACHE STRING "location of pylon sdk")

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--add-needed")
link_directories(${PYLON_ROOT}/genicam/bin/Linux64_x64)

set(basler_srcs src/BaslerCamera.cpp src/BaslerInterface.cpp src/BaslerDetInfoCtrlObj.cpp
                src/BaslerSyncCtrlObj.cpp src/BaslerRoiCtrlObj.cpp
                src/BaslerBinCtrlObj.cpp src/BaslerVideoCtrlObj.cpp)

file(STRINGS "VERSION" basler_ver)
add_library(limabasler SHARED
    ${basler_srcs}
)
TARGET_INCLUDE_DIRECTORIES(limabasler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
TARGET_INCLUDE_DIRECTORIES(limabasler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../common/include")
TARGET_INCLUDE_DIRECTORIES(limabasler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../hardware/include")
TARGET_INCLUDE_DIRECTORIES(limabasler PRIVATE "${PYLON_ROOT}/include")
TARGET_INCLUDE_DIRECTORIES(limabasler PRIVATE "${PYLON_ROOT}/genicam/library/CPP/include")
target_compile_definitions(limabasler PRIVATE USE_GIGE)
TARGET_LINK_LIBRARIES(limabasler limacore)
FIND_LIBRARY(PYLONBASE pylonbase ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
TARGET_LINK_LIBRARIES(limabasler ${PYLONBASE})
FIND_LIBRARY(PYLONGIGESUPP pylongigesupp ${PYLON_ROOT}/lib ${PYLON_ROOT}/lib64)
TARGET_LINK_LIBRARIES(limabasler ${PYLONGIGESUPP})
SET_TARGET_PROPERTIES(limabasler PROPERTIES VERSION ${basler_ver} SOVERSION "1")
INSTALL(TARGETS limabasler
        LIBRARY DESTINATION lib)
