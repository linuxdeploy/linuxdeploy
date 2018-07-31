# 3.5 is required for imported boost targets
cmake_minimum_required(VERSION 3.5)

# globally include own includes
include_directories(${PROJECT_SOURCE_DIR}/include)

if(USE_SYSTEM_BOOST)
    set(Boost_USE_STATIC_LIBS ON)
    find_package(Boost REQUIRED COMPONENTS filesystem regex)
    set(BOOST_LIBS Boost::filesystem Boost::regex)
else()
    # use custom built libs
    set(BOOST_LIBS boost_system boost_filesystem boost_regex)
endif()

add_subdirectory(util)
add_subdirectory(plugin)
add_subdirectory(core)

add_executable(linuxdeploy main.cpp)
target_link_libraries(linuxdeploy linuxdeploy_core args)
set_target_properties(linuxdeploy PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

add_executable(plugin_test plugin_test_main.cpp)
target_link_libraries(plugin_test linuxdeploy_plugin)
set_target_properties(plugin_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

add_executable(appdir_test appdir_test_main.cpp)
target_link_libraries(appdir_test linuxdeploy_core args)
target_include_directories(appdir_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/core)
set_target_properties(appdir_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
