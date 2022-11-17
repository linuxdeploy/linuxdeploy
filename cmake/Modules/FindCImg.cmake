#[=======================================================================[.rst:
FindCImg
-------

Finds the CImg library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``CImg``
  The CImg library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``CImg_FOUND``
  True if the system has the CImg library.
``CImg_VERSION``
  The version of the CImg library which was found.
``CImg_INCLUDE_DIRS``
  Include directories needed to use CImg.
``CImg_LIBRARIES``
  Libraries needed to link to CImg.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``CImg_INCLUDE_DIR``
  The directory containing ``CImg.h``.

#]=======================================================================]

# 3.6 required for PNG imported target
# 3.9 required for CMAKE_MATCH_<n>
cmake_minimum_required(VERSION 3.9)

find_path(CIMG_H_DIR
    NAMES CImg.h
    HINTS ${CMAKE_INSTALL_PREFIX} ${CIMG_DIR}
    PATH_SUFFIXES include include/linux
)

if(NOT CIMG_H_DIR)
    set(CImg_FOUND FALSE)
else()
    set(CImg_FOUND TRUE)

    find_package(PkgConfig)

    pkg_check_modules(libpng REQUIRED libpng)
    pkg_check_modules(libjpeg REQUIRED libjpeg)

    set(PNG_INCLUDE_DIR ${libpng_INCLUDE_DIRS})
    set(JPEG_INCLUDE_DIR ${libjpeg_INCLUDE_DIRS})

    if(STATIC_BUILD)
        set(PNG_LIBRARY ${libpng_STATIC_LIBRARIES})
        set(JPEG_LIBRARIES ${libjpeg_STATIC_LIBRARIES})
    else()
        set(PNG_LIBRARY ${libpng_LIBRARIES})
        set(JPEG_LIBRARIES ${libjpeg_LIBRARIES})
    endif()

    set(CImg_INCLUDE_DIR ${CIMG_H_DIR} CACHE STRING "")
    set(CImg_INCLUDE_DIRS ${CImg_INCLUDE_DIR})
    set(CImg_LIBRARIES "${PNG_LIBRARY};${JPEG_LIBRARIES}")
    set(CImg_DEFINITIONS "cimg_display=0;cimg_use_png=1;cimg_use_jpeg=1")

    file(READ "${CIMG_H_DIR}/CImg.h" header)
    string(REGEX MATCH "#define cimg_version ([0-9a-zA-Z\.]+)" _ "${header}")
    set(CImg_VERSION "${CMAKE_MATCH_1}")

    # CImg_VERSION

    if(NOT TARGET CImg)
        add_library(CImg INTERFACE)
        set_property(TARGET CImg PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CIMG_H_DIR};${PNG_INCLUDE_DIR};${JPEG_INCLUDE_DIR}")
        set_property(TARGET CImg PROPERTY INTERFACE_LINK_LIBRARIES "${CImg_LIBRARIES}")
        set_property(TARGET CImg PROPERTY INTERFACE_COMPILE_DEFINITIONS "${CImg_DEFINITIONS}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CImg
  FOUND_VAR CImg_FOUND
  REQUIRED_VARS
    CImg_INCLUDE_DIRS
    CImg_LIBRARIES
    CImg_DEFINITIONS
  VERSION_VAR CImg_VERSION
)
