include(CTest)

add_library(subprocess INTERFACE)
target_sources(subprocess INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cpp-subprocess/subprocess.hpp)
target_include_directories(subprocess INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cpp-subprocess)
target_compile_options(subprocess INTERFACE "-Wno-deprecated")

add_library(args INTERFACE)
target_sources(args INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/args/args.hxx)
target_include_directories(args INTERFACE args)

add_subdirectory(linuxdeploy-desktopfile)

function(ld_add_boost_headeronly_lib)
    set(options "")
    set(oneValueArgs TARGET_NAME ROOT_DIR)
    set(multiValueArgs LINK_LIBRARIES)
    cmake_parse_arguments(A "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

#    message(STATUS "Adding boost header-only library ${A_TARGET_NAME} in directory ${A_ROOT_DIR} with dependencies to ${A_LINK_LIBRARIES}")
    add_library(${A_TARGET_NAME} INTERFACE)
    set_property(TARGET ${A_TARGET_NAME} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${A_ROOT_DIR}/include>")
    set_property(TARGET ${A_TARGET_NAME} PROPERTY INTERFACE_LINK_LIBRARIES ${A_LINK_LIBRARIES})
endfunction()


if(NOT USE_SYSTEM_BOOST)
    ld_add_boost_headeronly_lib(TARGET_NAME boost_config ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/boost-config)

    ld_add_boost_headeronly_lib(TARGET_NAME boost_throw_exception ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/boost-throw_exception LINK_LIBRARIES boost_throw_exception)

    foreach(boost_lib_name utility predef assert core type_traits iterator mpl preprocessor static_assert detail smart_ptr exception io container_hash range integer)
        ld_add_boost_headeronly_lib(TARGET_NAME boost_${boost_lib_name} ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/boost-${boost_lib_name} LINK_LIBRARIES boost_config;boost_throw_exception)
    endforeach()

    ld_add_boost_headeronly_lib(TARGET_NAME boost_functional ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/boost-utility LINK_LIBRARIES boost_config;boost_container_hash)

    file(GLOB boost_system_srcs ${CMAKE_CURRENT_SOURCE_DIR}/boost-system/src/*.cpp)
    add_library(boost_system STATIC ${boost_system_srcs})
    target_include_directories(boost_system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/boost-system/include)
    target_link_libraries(boost_system PUBLIC
        boost_config boost_predef boost_assert boost_core boost_type_traits boost_iterator boost_mpl boost_preprocessor
        boost_static_assert boost_detail boost_smart_ptr boost_throw_exception boost_io boost_functional boost_range
    )

    file(GLOB boost_filesystem_srcs ${CMAKE_CURRENT_SOURCE_DIR}/boost-filesystem/src/*.cpp)
    add_library(boost_filesystem STATIC ${boost_filesystem_srcs})
    target_include_directories(boost_filesystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/boost-filesystem/include)
    target_link_libraries(boost_filesystem PUBLIC boost_config boost_utility boost_system boost_throw_exception)

    file(GLOB boost_regex_srcs ${CMAKE_CURRENT_SOURCE_DIR}/boost-regex/src/*.cpp)
    add_library(boost_regex STATIC ${boost_regex_srcs})
    target_include_directories(boost_regex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/boost-regex/include)
    target_link_libraries(boost_regex PUBLIC
        boost_config boost_predef boost_assert boost_throw_exception boost_smart_ptr boost_core boost_mpl
        boost_type_traits boost_static_assert boost_integer boost_preprocessor boost_functional boost_detail
    )
endif()

if(BUILD_TESTING AND NOT TARGET gtest)
    add_subdirectory(googletest)
endif()
