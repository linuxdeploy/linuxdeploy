set(headers_dir ${PROJECT_SOURCE_DIR}/include/linuxdeploy/log)

add_library(linuxdeploy_log OBJECT
    log.cpp
    ${headers_dir}/log.h
)
target_include_directories(linuxdeploy_log PUBLIC ${PROJECT_SOURCE_DIR}/include)
