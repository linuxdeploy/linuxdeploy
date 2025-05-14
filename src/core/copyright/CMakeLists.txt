cmake_minimum_required(VERSION 3.0)

add_library(linuxdeploy_core_copyright STATIC copyright.cpp copyright.h copyright_dpkgquery.cpp copyright_dpkgquery.h)

target_link_libraries(linuxdeploy_core_copyright PUBLIC linuxdeploy_util)
