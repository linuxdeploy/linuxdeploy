# required for PNG imported target
cmake_minimum_required(VERSION 3.5)

message(STATUS "Searching for CImg")

find_path(CIMG_H_DIR
    NAMES CImg.h
    HINTS ${CMAKE_INSTALL_PREFIX}
    PATH_SUFFIXES include include/linux
)

if(NOT CIMG_H_DIR)
    message(FATAL_ERROR "CImg.h not found")
endif()

find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)

add_library(CImg INTERFACE IMPORTED)
set_property(TARGET CImg PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CIMG_H_DIR};${JPEG_INCLUDE_DIR}")
set_property(TARGET CImg PROPERTY INTERFACE_LINK_LIBRARIES "PNG::PNG;${JPEG_LIBRARIES}")
set_property(TARGET CImg PROPERTY INTERFACE_COMPILE_DEFINITIONS "cimg_display=0;cimg_use_png=1;cimg_use_jpeg=1")
