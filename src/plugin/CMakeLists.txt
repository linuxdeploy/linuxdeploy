set(headers_dir ${PROJECT_SOURCE_DIR}/include/linuxdeploy/plugin/)

set(headers
    ${headers_dir}/plugin.h
    ${headers_dir}/base.h
    ${headers_dir}/base_impl.h
    ${headers_dir}/exceptions.h
    ${headers_dir}/plugin_process_handler.h
)

add_library(linuxdeploy_plugin STATIC
    plugin.cpp
    plugin_type0.cpp
    plugin_process_handler.cpp
    ${headers}
)
target_link_libraries(linuxdeploy_plugin PUBLIC linuxdeploy_core linuxdeploy_subprocess)

unset(headers)
unset(headers_dir)

install(TARGETS linuxdeploy_plugin)
