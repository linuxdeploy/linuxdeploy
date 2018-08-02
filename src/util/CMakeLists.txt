add_library(linuxdeploy_util STATIC
    magicwrapper.cpp
    magicwrapper.h
    ${PROJECT_SOURCE_DIR}/include/linuxdeploy/util/util.h
    ${PROJECT_SOURCE_DIR}/include/linuxdeploy/util/misc.h
)
target_include_directories(linuxdeploy_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include)
