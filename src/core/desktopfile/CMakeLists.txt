cmake_minimum_required(VERSION 3.0)

file(GLOB HEADERS ${PROJECT_SOURCE_DIR}/include/linuxdeploy/core/desktopfile/*.h)

add_library(linuxdeploy_core_desktopfile STATIC
    desktopfile.cpp
    desktopfileentry.cpp
    desktopfilereader.cpp
    desktopfilewriter.cpp
    desktopfilereader.h
    desktopfilewriter.h
    ${HEADERS}
)

target_include_directories(linuxdeploy_core_desktopfile PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(linuxdeploy_core_desktopfile PUBLIC linuxdeploy_util linuxdeploy_core_log ${BOOST_LIBS})
