# required for zlib imported target
cmake_minimum_required(VERSION 3.3)

message(STATUS "Searching for libmagic")

find_library(LIBMAGIC_A libmagic.a)

if(NOT LIBMAGIC_A)
    message(FATAL_ERROR "libmagic.a not found")
endif()

find_path(LIBMAGIC_MAGIC_H_DIR
    NAMES magic.h
    HINTS ${CMAKE_INSTALL_PREFIX}
    PATH_SUFFIXES include include/linux
)

if(NOT LIBMAGIC_MAGIC_H_DIR)
    message(FATAL_ERROR "magic.h not found")
endif()

# on Debian-ish systems, libmagic depends on zlib
find_package(ZLIB REQUIRED)

add_library(libmagic_static INTERFACE IMPORTED)
set_property(TARGET libmagic_static PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBMAGIC_MAGIC_H_DIR})
set_property(TARGET libmagic_static PROPERTY INTERFACE_LINK_LIBRARIES ${LIBMAGIC_A};ZLIB::ZLIB)
