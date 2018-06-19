# globally include own includes
include_directories(${PROJECT_SOURCE_DIR}/include)

if(USE_SYSTEM_BOOST)
    set(Boost_USE_STATIC_LIBS ON)
    find_package(Boost REQUIRED COMPONENTS filesystem regex)
    set(BOOST_LIBS Boost::filesystem Boost::regex)
else()
    # use custom built libs
    set(BOOST_LIBS Boost_system Boost_filesystem Boost_regex)
endif()

add_subdirectory(util)
add_subdirectory(plugin)
add_subdirectory(core)

add_executable(plugin_test plugin_test_main.cpp)
target_link_libraries(plugin_test linuxdeploy_plugin)
set_target_properties(plugin_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
