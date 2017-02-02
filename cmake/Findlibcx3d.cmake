# Try to find libcx3d (*shared* or *static* CyberX3D VRML library)
# Once done this will define
#
#  LIBCX3D_FOUND - system has libcx3d
#  LIBCX3D_INCLUDE_DIRS - the libcx3d include directory
#  LIBCX3D_LIBRARIES - Link these to use libcx3d
#
#  Adapted from cmake-modules Google Code project
#
#  Copyright (c) 2006 Andreas Schneider <mail@cynapses.org>
#
#  (Changes for libcx3d) Copyright (c) 2013 CompuPhase
#
# Redistribution and use is allowed according to the terms of the New BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (LIBCX3D_LIBRARIES AND LIBCX3D_INCLUDE_DIRS)
  # in cache already
  set(LIBCX3D_FOUND TRUE)
else (LIBCX3D_LIBRARIES AND LIBCX3D_INCLUDE_DIRS)
  find_path(LIBCX3D_INCLUDE_DIR
    NAMES
    CyberX3D.h
    PATHS
    /usr/include
    /usr/include/CyberX3D-1.0/cybergarage/x3d
    /usr/local/include
    /usr/local/include/CyberX3D-1.0/cybergarage/x3d
    /opt/local/include
    /opt/local/include/CyberX3D-1.0/cybergarage/x3d
    /sw/include
    /sw/include/CyberX3D-1.0/cybergarage/x3d
    )

  SET(CX3D_LIBNAME cx3d-1.0)
  find_library(LIBCX3D_LIBRARY
    NAMES
    ${CX3D_LIBNAME}
    PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    /usr/lib64
    /usr/local/lib64
    /opt/local/lib64
    /sw/lib64
    )

  if(LIBCX3D_INCLUDE_DIR)
    set(LIBCX3D_INCLUDE_DIRS
      ${LIBCX3D_INCLUDE_DIR}/../..
      )
  endif(LIBCX3D_INCLUDE_DIR)
  set(LIBCX3D_LIBRARIES
    ${LIBCX3D_LIBRARY}
    )

  if (LIBCX3D_INCLUDE_DIRS AND LIBCX3D_LIBRARIES)
    set(LIBCX3D_FOUND TRUE)
  endif (LIBCX3D_INCLUDE_DIRS AND LIBCX3D_LIBRARIES)

  if (LIBCX3D_FOUND)
    if (NOT libcx3d_FIND_QUIETLY)
      message(STATUS "Found libcx3d: ${LIBCX3D_LIBRARIES} / ${LIBCX3D_INCLUDE_DIRS}")
    endif (NOT libcx3d_FIND_QUIETLY)
  else (LIBCX3D_FOUND)
    if (libcx3d_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find libcx3d")
    endif (libcx3d_FIND_REQUIRED)
  endif (LIBCX3D_FOUND)

  # show the LIBCX3D_INCLUDE_DIRS and LIBCX3D_LIBRARIES variables only in the advanced view
  mark_as_advanced(LIBCX3D_INCLUDE_DIRS LIBCX3D_LIBRARIES)

endif (LIBCX3D_LIBRARIES AND LIBCX3D_INCLUDE_DIRS)
