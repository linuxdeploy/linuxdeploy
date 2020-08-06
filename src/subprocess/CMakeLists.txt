set(headers_dir ${PROJECT_SOURCE_DIR}/include/linuxdeploy/subprocess)

add_library(linuxdeploy_subprocess STATIC
    subprocess.cpp
    subprocess_result.cpp
    ${headers_dir}/subprocess.h
    ${headers_dir}/subprocess_result.h
)
target_include_directories(linuxdeploy_subprocess PUBLIC ${headers_dir})

add_executable(subprocess_demo subprocess_demo.cpp)
target_link_libraries(subprocess_demo PUBLIC linuxdeploy_subprocess)
