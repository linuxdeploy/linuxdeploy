cmake_minimum_required(VERSION 2.8)

# include headers to make CLion happy
file(GLOB HEADERS ${PROJECT_SOURCE_DIR}/include/linuxdeploy/core/*.h)

find_package(Threads)

find_package(CImg REQUIRED)

message(STATUS "Generating excludelist")
execute_process(
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/generate-excludelist.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_library(linuxdeploy_core_log STATIC log.cpp)
target_include_directories(linuxdeploy_core_log PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(linuxdeploy_core_log PUBLIC ${BOOST_LIBS})

add_subdirectory(copyright)

add_library(linuxdeploy_core STATIC elf_file.cpp appdir.cpp ${HEADERS} appdir_root_setup.cpp)
target_link_libraries(linuxdeploy_core PUBLIC
    linuxdeploy_plugin linuxdeploy_core_log linuxdeploy_util linuxdeploy_desktopfile_static
    ${BOOST_LIBS} CImg ${CMAKE_THREAD_LIBS_INIT}
)
target_link_libraries(linuxdeploy_core PRIVATE linuxdeploy_core_copyright)
target_include_directories(linuxdeploy_core PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(linuxdeploy_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(linuxdeploy_core PUBLIC -DBOOST_NO_CXX11_SCOPED_ENUMS)
