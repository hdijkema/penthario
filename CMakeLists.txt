cmake_minimum_required(VERSION 3.24)

#########################################################################################################################
# PGADMIN_JR version
#########################################################################################################################

set(PGADM_RELEASE_VERSION "" CACHE STRING "Release version (major.minor.patch); empty uses the development version")
set(PGADM_VERSION "0.3.6")
if(NOT PGADM_RELEASE_VERSION STREQUAL "")
    if(NOT PGADM_RELEASE_VERSION MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
        message(FATAL_ERROR "PGADM_RELEASE_VERSION must have the form major.minor.patch")
    endif()
    set(PGADM_VERSION "${PGADM_RELEASE_VERSION}")
endif()
project(pgadmin-jr VERSION ${PGADM_VERSION} LANGUAGES C CXX)
# The normal application and every release package are Qt-free.
option(PGADM_BUILD_QT_LEGACY "Build the historical Qt implementation instead of the normal application" OFF)
if(DEFINED PGADM_BUILD_FLTK)
    message(STATUS "PGADM_BUILD_FLTK=${PGADM_BUILD_FLTK} is obsolete; the normal application always uses FLTK")
endif()
if(PGADM_BUILD_QT_LEGACY)
    include(cmake/QtLegacy.cmake)
else()
    include(cmake/FltkApplication.cmake)
endif()
