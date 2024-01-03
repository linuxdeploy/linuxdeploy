set(headers_dir ${PROJECT_SOURCE_DIR}/include/linuxdeploy/subprocess)

add_library(linuxdeploy_subprocess STATIC
    subprocess.cpp
    subprocess_result.cpp
    process.cpp
    pipe_reader.cpp
    util.cpp
    ${headers_dir}/subprocess.h
    ${headers_dir}/subprocess_result.h
    ${headers_dir}/process.h
    ${headers_dir}/pipe_reader.h
    ${headers_dir}/util.h
)
target_include_directories(linuxdeploy_subprocess PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(linuxdeploy_subprocess PUBLIC linuxdeploy_log)

add_executable(subprocess_demo subprocess_demo.cpp)
target_link_libraries(subprocess_demo PUBLIC linuxdeploy_subprocess)
