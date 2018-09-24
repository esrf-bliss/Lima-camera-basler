set(PYLON_INCLUDE_DIRS)
set(PYLON_LIBRARIES)
set(PYLON_DEFINITIONS)

if(WIN32)
  find_library(PYLONGUI_LIB PylonGUI${PYLON_SUFFIX} ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
  find_library(PYLONUTILITY_LIB PylonUtility${PYLON_SUFFIX} ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
  find_library(PYLONBOOTSTRAPPER_LIB PylonBootstrapper ${PYLON_ROOT}/lib/Win32 ${PYLON_ROOT}/lib/x64)
  find_library(PYLONGCBASE_LIB GCBase${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
  find_library(PYLONGENAPI_LIB GenApi${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
  find_library(PYLONMATHPARSER_LIB MathParser${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
  find_library(PYLONLOG_LIB Log${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
  find_library(PYLONLOG4CPP_LIB log4cpp${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
  find_library(PYLONLOG4CPPSTATIC_LIB log4cpp-static${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
  find_library(PYLONCLALLSERIAL_LIB CLAllSerial${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
  find_library(PYLONCLPROTOCOL_LIB CLProtocol${PYLON_SUFFIX}_v2_3 ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
  find_library(PYLONCLSERCOM_LIB CLSerCOM ${PYLON_ROOT}/../genicam/library/cpp/lib/win32_i86 ${PYLON_ROOT}/../genicam/library/cpp/lib/win64_x64)
  set(PYLON_LIBRARIES REQUIRED_VARS
    PYLONGUI_LIB PYLONUTILITY_LIB PYLONBOOTSTRAPPER_LIB PYLONGCBASE_LIB PYLONGENAPI_LIB PYLONMATHPARSER_LIB PYLONLOG_LIB PYLONLOG4CPP_LIB PYLONLOG4CPPSTATIC_LIB
    PYLONCLALLSERIAL_LIB PYLONCLPROTOCOL_LIB PYLONCLSERCOM_LIB
endif()

if(WIN32)
  list(APPEND PYLON_INCLUDE_DIRS "${PYLON_ROOT}/../genicam/library/cpp/include")
else()
  list(APPEND PYLON_INCLUDE_DIRS "${PYLON_ROOT}/genicam/library/CPP/include")
endif()

list(APPEND PYLON_DEFINITIONS USE_GIGE)
if(WIN32)
    list(APPEND PYLON_DEFINITIONS LIBBASLER_EXPORTS)
endif()

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

if (WIN32)
  list(APPEND PYLON_LIBRARIES ws2_32)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Pylon3 DEFAULT_MSG
  PYLON_LIBRARIES
  PYLON_INCLUDE_DIRS
)
