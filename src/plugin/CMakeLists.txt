file(GLOB PLUGIN_HEADERS ${PROJECT_SOURCE_DIR}/include/linuxdeploy/plugin/*.h)

add_library(linuxdeploy_plugin STATIC plugin_type0.cpp plugin.cpp ${PLUGIN_HEADERS})
target_link_libraries(linuxdeploy_plugin PUBLIC linuxdeploy_core ${BOOST_LIBS} subprocess)
