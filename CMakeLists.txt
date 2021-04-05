cmake_minimum_required (VERSION 2.6)

project (x3d-xrdesktop)
set (x3d_VERSION_MAJOR 0)
set (x3d_VERSION_MINOR 1)

# set the default install to our binary directory
#set (CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}/release" CACHE PATH "install prefix" FORCE)
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set (CMAKE_INSTALL_PREFIX
    "${PROJECT_BINARY_DIR}/release" CACHE PATH "x3d install prefix" FORCE)
endif (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

configure_file (
  "${PROJECT_SOURCE_DIR}/x3dConfig.h.in"
  "${PROJECT_BINARY_DIR}/x3dConfig.h"
  )
include_directories("${PROJECT_BINARY_DIR}")

set (EXTRA_LIBS X11 GLEW GL Xdamage)

include (FindPkgConfig)
PKG_CHECK_MODULES (KWINXRDEPS REQUIRED
    xrdesktop-0.15
    graphene-1.0
    libinputsynth-0.15
)

include_directories ("${PROJECT_SOURCE_DIR}/vertex")
add_subdirectory (vertex)
set (EXTRA_LIBS ${EXTRA_LIBS} vertex)

include_directories ("${PROJECT_SOURCE_DIR}/xman")
add_subdirectory (xman)
set (EXTRA_LIBS ${EXTRA_LIBS} xman)

add_executable (x3d-xrdesktop phasetest.cpp)

target_link_libraries (x3d-xrdesktop
  ${EXTRA_LIBS}
  ${KWINXRDEPS_LIBRARIES}
  ${KWINXRINPUTDEPS_LIBRARIES}
)

message ("Include: ${KWINXRDEPS_INCLUDE_DIRS}")
message ("Link: ${KWINXRDEPS_LIBRARIES}")

target_include_directories(x3d-xrdesktop PRIVATE
    ${KWINXRDEPS_INCLUDE_DIRS}
    ${KWINXRINPUTDEPS_INCLUDE_DIRS}
)

install (TARGETS x3d-xrdesktop DESTINATION bin)

add_test (x3dRuns x3d -display :9)

