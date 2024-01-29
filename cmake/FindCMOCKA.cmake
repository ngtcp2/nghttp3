# - Try to find cmocka
# Once done this will define
#  CMOCKA_FOUND        - System has cmocka
#  CMOCKA_INCLUDE_DIRS - The cmocka include directories
#  CMOCKA_LIBRARIES    - The libraries needed to use cmocka

find_package(PkgConfig QUIET)
pkg_check_modules(PC_CMOCKA QUIET cmocka)

find_path(CMOCKA_INCLUDE_DIR
  NAMES cmocka.h
  HINTS ${PC_CMOCKA_INCLUDE_DIRS}
)
find_library(CMOCKA_LIBRARY
  NAMES cmocka
  HINTS ${PC_CMOCKA_LIBRARY_DIRS}
)

if(PC_CMOCKA_FOUND)
  set(CMOCKA_VERSION ${PC_CMOCKA_VERSION})
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set CMOCKA_FOUND to TRUE
# if all listed variables are TRUE and the requested version matches.
find_package_handle_standard_args(CMOCKA REQUIRED_VARS
                                  CMOCKA_LIBRARY CMOCKA_INCLUDE_DIR
                                  VERSION_VAR CMOCKA_VERSION)

if(CMOCKA_FOUND)
  set(CMOCKA_LIBRARIES     ${CMOCKA_LIBRARY})
  set(CMOCKA_INCLUDE_DIRS  ${CMOCKA_INCLUDE_DIR})
endif()

mark_as_advanced(CMOCKA_INCLUDE_DIR CMOCKA_LIBRARY)
