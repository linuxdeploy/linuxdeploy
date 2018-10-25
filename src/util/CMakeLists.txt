add_library(linuxdeploy_util STATIC subprocess.cpp)
target_sources(linuxdeploy_util INTERFACE
    ${PROJECT_SOURCE_DIR}/include/linuxdeploy/util/util.h
    ${PROJECT_SOURCE_DIR}/include/linuxdeploy/util/misc.h
)
target_include_directories(linuxdeploy_util INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(linuxdeploy_util PUBLIC subprocess)
