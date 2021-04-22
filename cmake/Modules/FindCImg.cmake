# required for PNG imported target
cmake_minimum_required(VERSION 3.6)

find_package(PkgConfig)
pkg_check_modules(libpng REQUIRED IMPORTED_TARGET libpng)

# build custom static version of libjpeg for static builds
if(STATIC_BUILD)
    include(ExternalProject)

    if($ENV{ARCH} MATCHES "i[356]86")
        set(configure_command_prefix env CFLAGS=-m32 CXXFLAGS=-m32)
    endif()

    ExternalProject_Add(libjpeg_static_extproj
        URL https://www.ijg.org/files/jpegsrc.v9d.tar.gz
        URL_HASH SHA256=6c434a3be59f8f62425b2e3c077e785c9ce30ee5874ea1c270e843f273ba71ee
        BUILD_IN_SOURCE ON
        EXCLUDE_FROM_ALL ON
        CONFIGURE_COMMAND ${configure_command_prefix} ./configure --prefix=/usr
        INSTALL_COMMAND ""
    )

    ExternalProject_Get_property(libjpeg_static_extproj SOURCE_DIR)
    add_library(libjpeg_static INTERFACE IMPORTED)
    set_property(TARGET libjpeg_static PROPERTY INTERFACE_LINK_LIBRARIES ${SOURCE_DIR}/.libs/libjpeg.a)
    add_dependencies(libjpeg_static libjpeg_static_extproj)

    set(JPEG_LIBRARIES libjpeg_static)
else()
    find_package(JPEG REQUIRED)
endif()

if(NOT USE_SYSTEM_CIMG)
    message(STATUS "Using bundled CImg library")

    set(CIMG_H_DIR "${PROJECT_SOURCE_DIR}/lib/CImg/")
else()
    message(STATUS "Searching for CImg")

    find_path(CIMG_H_DIR
        NAMES CImg.h
        HINTS ${CMAKE_INSTALL_PREFIX}
        PATH_SUFFIXES include include/linux
    )

    if(NOT CIMG_H_DIR)
        message(FATAL_ERROR "CImg.h not found")
    endif()
endif()

set(PNG_INCLUDE_DIR ${libpng_INCLUDE_DIRS})
if(NOT STATIC_BUILD)
    set(PNG_LIBRARY ${libpng_LIBRARIES})
else()
    set(PNG_LIBRARY ${libpng_STATIC_LIBRARIES})
endif()

#message(FATAL_ERROR "${PNG_LIBRARY}")

add_library(CImg INTERFACE)
set_property(TARGET CImg PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CIMG_H_DIR};${PNG_INCLUDE_DIR};${JPEG_INCLUDE_DIR}")
set_property(TARGET CImg PROPERTY INTERFACE_LINK_LIBRARIES "${PNG_LIBRARY};${JPEG_LIBRARIES}")
set_property(TARGET CImg PROPERTY INTERFACE_COMPILE_DEFINITIONS "cimg_display=0;cimg_use_png=1;cimg_use_jpeg=1")
