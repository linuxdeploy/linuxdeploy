cmake_minimum_required(VERSION 3.6)

set(headers_dir ${PROJECT_SOURCE_DIR}/include/linuxdeploy/util/)

add_library(linuxdeploy_util INTERFACE)
target_sources(linuxdeploy_util INTERFACE
    ${headers_dir}/misc.h
    ${headers_dir}/util.h
)
target_include_directories(linuxdeploy_util INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include)
