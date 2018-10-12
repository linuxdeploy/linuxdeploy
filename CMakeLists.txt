cmake_minimum_required(VERSION 3.2)

project(linuxdeploy C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")

set(USE_SYSTEM_BOOST OFF CACHE BOOL "Set to ON to use system boost libraries instead of building up to date boost libraries from source")
set(USE_SYSTEM_CIMG ON CACHE BOOL "Set to OFF to use CImg library bundled in lib directory")

add_subdirectory(lib)

add_subdirectory(src)