# required for PNG imported target
cmake_minimum_required(VERSION 3.6)

find_package(PkgConfig)
pkg_check_modules(libpng REQUIRED IMPORTED_TARGET libpng)
pkg_check_modules(libjpeg REQUIRED IMPORTED_TARGET libjpeg)

message(STATUS "Searching for CImg")

find_path(CIMG_H_DIR
    NAMES CImg.h
    HINTS ${CMAKE_INSTALL_PREFIX}
    PATH_SUFFIXES include include/linux
)

if(NOT CIMG_H_DIR)
    message(FATAL_ERROR "CImg.h not found")
endif()

set(PNG_INCLUDE_DIR ${libpng_INCLUDE_DIRS})
set(PNG_INCLUDE_DIR ${libjpeg_INCLUDE_DIRS})

if(NOT STATIC_BUILD)
    set(PNG_LIBRARY ${libpng_LIBRARIES})
    set(JPEG_LIBRARIES ${libjpeg_LIBRARIES})
else()
    set(PNG_LIBRARY ${libpng_STATIC_LIBRARIES})
    set(JPEG_LIBRARIES ${libjpeg_STATIC_LIBRARIES})
endif()

add_library(CImg INTERFACE)
set_property(TARGET CImg PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CIMG_H_DIR};${PNG_INCLUDE_DIR};${JPEG_INCLUDE_DIR}")
set_property(TARGET CImg PROPERTY INTERFACE_LINK_LIBRARIES "${PNG_LIBRARY};${JPEG_LIBRARIES}")
set_property(TARGET CImg PROPERTY INTERFACE_COMPILE_DEFINITIONS "cimg_display=0;cimg_use_png=1;cimg_use_jpeg=1")
