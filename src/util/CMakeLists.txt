cmake_minimum_required(VERSION 3.0)

file(GLOB HEADERS ${PROJECT_SOURCE_DIR}/include/linuxdeploy/util/*.h)

add_library(linuxdeploy_util STATIC subprocess.cpp ${HEADERS})
target_include_directories(linuxdeploy_util INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(linuxdeploy_util PUBLIC subprocess)
