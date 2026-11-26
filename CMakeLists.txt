cmake_minimum_required(VERSION 3.24)

#########################################################################################################################
# PENTHARIO version
#########################################################################################################################

set(PGADM_RELEASE_VERSION "" CACHE STRING "Release version (major.minor.patch); empty uses the development version")
set(PGADM_VERSION "0.4.5")
if(NOT PGADM_RELEASE_VERSION STREQUAL "")
    if(NOT PGADM_RELEASE_VERSION MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
        message(FATAL_ERROR "PGADM_RELEASE_VERSION must have the form major.minor.patch")
    endif()
    set(PGADM_VERSION "${PGADM_RELEASE_VERSION}")
endif()
project(penthario VERSION ${PGADM_VERSION} LANGUAGES C CXX)
include(cmake/FltkApplication.cmake)
