set(PYLON_LIBRARIES)
set(PYLON_DEFINITIONS)

# Pylon base
if (WIN32)
  set(PYLON_SUFFIX "_MD_VC141" CACHE STRING "Suffix used in Windows for naming Pylon files")
  find_library(PYLONBASE_LIB PylonBase_v6_2)
  find_path(PYLON_INCLUDE_DIRS "pylon/PylonBase.h")
else()
  find_library(PYLONBASE_LIB pylonbase ${PYLON_ROOT}/lib)
  find_path(PYLON_INCLUDE_DIRS "pylon/PylonBase.h")
endif()
list(APPEND PYLON_LIBRARIES ${PYLONBASE_LIB})

if (("${BASLER_PYLONBASE_LIB}" STREQUAL "BASLER_PYLONBASE_LIB-NOTFOUND"))
  message(FATAL_ERROR "${NAME} : pylonbase not found")
endif()

# Pylon additional Windows libraries
if(WIN32)
  find_library(PYLON_GUI_LIB PylonGUI_v6_2)
  find_library(PYLON_UTILITY_LIB PylonUtility_v6_2)
  find_library(PYLON_GCBASE_LIB GCBase${PYLON_SUFFIX}_v3_1_Basler_pylon)
  find_library(PYLON_GENAPI_LIB GenApi${PYLON_SUFFIX}_v3_1_Basler_pylon)
  list(APPEND PYLON_LIBRARIES ${PYLON_GUI_LIB} ${PYLON_UTILITY_LIB} ${PYLON_GCBASE_LIB} ${PYLON_GENAPI_LIB})
endif()

list(APPEND PYLON_DEFINITIONS USE_GIGE)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Pylon6 DEFAULT_MSG
  PYLON_LIBRARIES
  PYLON_INCLUDE_DIRS
)

if (WIN32)
  list(APPEND PYLON_LIBRARIES ws2_32)
  list(APPEND PYLON_DEFINITIONS _WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

if (PYLON_FIND_DEBUG)
  message(STATUS "PYLON_DIR: ${PYLON_DIR}")
  message(STATUS "PYLON_DEFINITIONS: ${PYLON_DEFINITIONS}")
  message(STATUS "PYLON_INCLUDE_DIRS: ${PYLON_INCLUDE_DIRS}")
  message(STATUS "PYLON_LIBRARIES: ${PYLON_LIBRARIES}")
endif()
