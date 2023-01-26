#.rst:
# FindReadline
# ------------
#
# Find readline include dirs and libraries.
#
# Imported Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines the following :ref:`Imported Targets <Imported Targets>`:
#
# ``Readline::history``
#
# ``Readline::readline``
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ``Readline_FOUND``
#
# ``Readline_INCLUDE_DIRS``
#
# ``Readline_LIBRARIES``
#
# ``Readline_VERSION``
#
# ``Readline_VERSION_MAJOR``
#
# ``Readline_VERSION_MINOR``
#
# ``Readline_history_FOUND``
#
# ``Readline_readline_FOUND``
#
# Cache variables
# ^^^^^^^^^^^^^^^
#
# Search results are saved persistently in CMake cache entries:
#
# ``Readline_INCLUDE_DIR``
#
# ``Readline_history_LIBRARY``
#
# ``Readline_readline_LIBRARY``
#
# ``Readline_readline_LIBRARY_DLL``
#
include(${CMAKE_ROOT}/Modules/FindPackageHandleStandardArgs.cmake)
find_package(PkgConfig QUIET)

set(PACKAGE Readline)
if(NOT ${PACKAGE}_FIND_COMPONENTS)
  set(${PACKAGE}_FIND_COMPONENTS readline)
  set(${PACKAGE}_FIND_REQUIRED_readline 1)
endif()

if(PKG_CONFIG_FOUND)
  list(FIND ${PACKAGE}_FIND_COMPONENTS history output)
  if(${output} GREATER -1)
    if(CMAKE_VERSION VERSION_GREATER 3.6)
      pkg_check_modules(history QUIET IMPORTED_TARGET history)
    else()
      pkg_check_modules(history QUIET history)
    endif()
  endif()
  if(history_FOUND)
    if(history_VERSION)
      set(${PACKAGE}_VERSION ${history_VERSION})
    endif()
  endif()
  list(FIND ${PACKAGE}_FIND_COMPONENTS readline output)
  if(${output} GREATER -1)
    if(CMAKE_VERSION VERSION_GREATER 3.6)
      pkg_check_modules(readline QUIET IMPORTED_TARGET readline)
    else()
      pkg_check_modules(readline QUIET readline)
    endif()
  endif()
  if(readline_FOUND)
    if(readline_VERSION)
      set(${PACKAGE}_VERSION ${readline_VERSION})
    endif()
    if(readline_CFLAGS)
      set(${PACKAGE}_CFLAGS ${readline_CFLAGS})
    endif()
  endif()
  set(output)
endif()

find_path(${PACKAGE}_INCLUDE_DIR
  NAMES
    readline/readline.h
    readline/history.h
  HINTS
    ${readline_INCLUDE_DIRS}
    ${history_INCLUDE_DIRS}
)
mark_as_advanced(${PACKAGE}_INCLUDE_DIR)
if(EXISTS ${${PACKAGE}_INCLUDE_DIR})
  list(APPEND ${PACKAGE}_INCLUDE_DIRS ${${PACKAGE}_INCLUDE_DIR})
endif()

if(NOT ${PACKAGE}_VERSION AND EXISTS ${${PACKAGE}_INCLUDE_DIR}/readline/readline.h)
  file(STRINGS ${${PACKAGE}_INCLUDE_DIR}/readline/readline.h text REGEX ".*READLINE_VERSION.*")
  string(REGEX REPLACE ".*0x([0-9]+).*" "\\1" ${PACKAGE}_VERSION "${text}")
  math(EXPR ${PACKAGE}_VERSION_MAJOR "${${PACKAGE}_VERSION}/100")
  math(EXPR ${PACKAGE}_VERSION_MINOR "${${PACKAGE}_VERSION}%100")
  set(${PACKAGE}_VERSION
    ${${PACKAGE}_VERSION_MAJOR}.${${PACKAGE}_VERSION_MINOR}
  )
  set(text)
endif()

list(FIND ${PACKAGE}_FIND_COMPONENTS readline output)
if(${output} GREATER -1 AND WIN32)
  foreach(path ${CMAKE_PREFIX_PATH} ${CMAKE_LIBRARY_PATH} ${CMAKE_FIND_ROOT_PATH})
    string(REGEX MATCH "[dD]ebug" output "${path}")
    if(output)
      find_file(${PACKAGE}_readline_LIBRARY_DEBUG_DLL NAMES readline.dll
        PATH_SUFFIXES bin HINTS ${path} NO_DEFAULT_PATH
      )
      mark_as_advanced(${PACKAGE}_readline_LIBRARY_DEBUG_DLL)
      find_library(${PACKAGE}_readline_LIBRARY_DEBUG NAMES readline
        PATH_SUFFIXES lib HINTS ${path} NO_DEFAULT_PATH
      )
      mark_as_advanced(${PACKAGE}_readline_LIBRARY_DEBUG)
    else()
      find_file(${PACKAGE}_readline_LIBRARY_DLL NAMES readline.dll
        PATH_SUFFIXES bin HINTS ${path} NO_DEFAULT_PATH
      )
      mark_as_advanced(${PACKAGE}_readline_LIBRARY_DLL)
      find_library(${PACKAGE}_readline_LIBRARY NAMES readline
        PATH_SUFFIXES lib HINTS ${path} NO_DEFAULT_PATH
      )
      mark_as_advanced(${PACKAGE}_readline_LIBRARY)
    endif()
  endforeach()
  if(${PACKAGE}_readline_LIBRARY)
    list(APPEND ${PACKAGE}_LIBRARIES ${${PACKAGE}_readline_LIBRARY})
  elseif(${PACKAGE}_readline_LIBRARY_DLL)
    list(APPEND ${PACKAGE}_LIBRARIES ${${PACKAGE}_readline_LIBRARY_DLL})
  elseif(${PACKAGE}_readline_LIBRARY_DEBUG)
    list(APPEND ${PACKAGE}_LIBRARIES ${${PACKAGE}_readline_LIBRARY_DEBUG})
  elseif(${PACKAGE}_readline_LIBRARY_DEBUG_DLL)
    list(APPEND ${PACKAGE}_LIBRARIES ${${PACKAGE}_readline_LIBRARY_DEBUG_DLL})
  endif()
endif()
foreach(module ${${PACKAGE}_FIND_COMPONENTS} ${readline_STATIC_LIBRARIES})
  find_library(${PACKAGE}_${module}_LIBRARY
    NAMES ${module} NAMES_PER_DIR
    HINTS
      ${readline_LIBRARY_DIRS}
      ${history_LIBRARY_DIRS}
  )
  mark_as_advanced(${PACKAGE}_${module}_LIBRARY)
  if(EXISTS ${${PACKAGE}_${module}_LIBRARY})
    list(APPEND ${PACKAGE}_LIBRARIES ${${PACKAGE}_${module}_LIBRARY})
    set(${PACKAGE}_${module}_FOUND 1)
  endif()
endforeach()
if(${PACKAGE}_LIBRARIES)
  list(REMOVE_DUPLICATES ${PACKAGE}_LIBRARIES)
endif()
set(output)

list(APPEND ${PACKAGE}_REQUIRED_VARS ${PACKAGE}_LIBRARIES ${PACKAGE}_INCLUDE_DIRS)
foreach(module ${${PACKAGE}_FIND_COMPONENTS})
  if(${PACKAGE}_FIND_REQUIRED_${module})
    list(APPEND ${PACKAGE}_REQUIRED_VARS ${PACKAGE}_${module}_LIBRARY)
  endif()
endforeach()
find_package_handle_standard_args(Readline
  FOUND_VAR
    ${PACKAGE}_FOUND
  REQUIRED_VARS
    ${${PACKAGE}_REQUIRED_VARS}
  VERSION_VAR
    ${PACKAGE}_VERSION
  HANDLE_COMPONENTS
)
set(${PACKAGE}_REQUIRED_VARS)

if(TARGET PkgConfig::history)
  add_library(${PACKAGE}::history ALIAS PkgConfig::history)
endif()
if(TARGET PkgConfig::readline)
  add_library(${PACKAGE}::readline ALIAS PkgConfig::readline)
endif()
if(TARGET PkgConfig::readline AND TARGET PkgConfig::history)
  set_property(TARGET PkgConfig::readline APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES PkgConfig::history
  )
endif()
if(NOT TARGET ${PACKAGE}::readline AND ${PACKAGE}_readline_FOUND)
  if(EXISTS "${${PACKAGE}_readline_LIBRARY_DEBUG_DLL}" OR EXISTS "${${PACKAGE}_readline_LIBRARY_DLL}")
    add_library(${PACKAGE}::readline SHARED IMPORTED)
    if(EXISTS "${${PACKAGE}_readline_LIBRARY_DEBUG_DLL}")
      set_target_properties(${PACKAGE}::readline PROPERTIES
        IMPORTED_LOCATION_DEBUG "${${PACKAGE}_readline_LIBRARY_DEBUG_DLL}"
      )
    endif()
    if(EXISTS "${${PACKAGE}_readline_LIBRARY_DEBUG}")
      set_target_properties(${PACKAGE}::readline PROPERTIES
        IMPORTED_IMPLIB_DEBUG "${${PACKAGE}_readline_LIBRARY_DEBUG}"
      )
    endif()
    if(EXISTS "${${PACKAGE}_readline_LIBRARY_DLL}")
      set_target_properties(${PACKAGE}::readline PROPERTIES
        IMPORTED_LOCATION_RELEASE "${${PACKAGE}_readline_LIBRARY_DLL}"
      )
    endif()
    if(EXISTS "${${PACKAGE}_readline_LIBRARY}")
      set_target_properties(${PACKAGE}::readline PROPERTIES
        IMPORTED_IMPLIB_RELEASE "${${PACKAGE}_readline_LIBRARY}"
      )
    endif()
  elseif(EXISTS "${${PACKAGE}_readline_LIBRARY_DEBUG}" OR EXISTS "${${PACKAGE}_readline_LIBRARY}")
    add_library(${PACKAGE}::readline STATIC IMPORTED)
    if(EXISTS "${${PACKAGE}_readline_LIBRARY_DEBUG}")
      set_target_properties(${PACKAGE}::readline PROPERTIES
        IMPORTED_LOCATION_DEBUG "${${PACKAGE}_readline_LIBRARY_DEBUG}"
      )
    endif()
    if(EXISTS "${${PACKAGE}_readline_LIBRARY}")
      set_target_properties(${PACKAGE}::readline PROPERTIES
        IMPORTED_LOCATION_RELEASE "${${PACKAGE}_readline_LIBRARY}"
      )
    endif()
    set_property(TARGET ${PACKAGE}::readline APPEND PROPERTY
      INTERFACE_COMPILE_OPTIONS -DREADLINE_STATIC
    )
  else()
    add_library(${PACKAGE}::readline UNKNOWN IMPORTED)
  endif()
  set_target_properties(${PACKAGE}::readline PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${${PACKAGE}_INCLUDE_DIR}"
    IMPORTED_LINK_INTERFACE_LANGUAGES C
  )
  if(${PACKAGE}_CFLAGS)
    set_property(TARGET ${PACKAGE}::readline APPEND PROPERTY
      INTERFACE_COMPILE_OPTIONS ${${PACKAGE}_CFLAGS}
    )
  endif()
  if(EXISTS "${${PACKAGE}_readline_LIBRARY}")
    set_target_properties(${PACKAGE}::readline PROPERTIES
      IMPORTED_LOCATION "${${PACKAGE}_readline_LIBRARY}"
    )
  endif()
endif()
foreach(module history ${readline_STATIC_LIBRARIES})
  if(NOT TARGET ${PACKAGE}::${module} AND ${PACKAGE}_${module}_FOUND)
    add_library(${PACKAGE}::${module} UNKNOWN IMPORTED)
    set_target_properties(${PACKAGE}::${module} PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${${PACKAGE}_INCLUDE_DIR}"
      IMPORTED_LOCATION "${${PACKAGE}_${module}_LIBRARY}"
      IMPORTED_LINK_INTERFACE_LANGUAGES C
    )
    if(NOT TARGET PkgConfig::readline AND ${PACKAGE}_readline_FOUND)
      set_property(TARGET ${PACKAGE}::readline APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES ${PACKAGE}::${module}
      )
    elseif(TARGET PkgConfig::readline AND ${PACKAGE}_readline_FOUND)
      set_property(TARGET PkgConfig::readline APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES ${PACKAGE}::${module}
      )
    endif()
  endif()
endforeach()
set(PACKAGE)
