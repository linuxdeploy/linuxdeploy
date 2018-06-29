add_library(subprocess INTERFACE)
target_sources(subprocess INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cpp-subprocess/subprocess.hpp)
target_include_directories(subprocess INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cpp-subprocess)

add_library(args INTERFACE)
target_sources(args INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/args/args.hxx)
target_include_directories(args INTERFACE args)

add_library(cpp-feather-ini-parser INTERFACE)
target_sources(cpp-feather-ini-parser INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cpp-feather-ini-parser/INI.h)
target_include_directories(cpp-feather-ini-parser INTERFACE cpp-feather-ini-parser)

add_executable(test_cpp_feather_ini_parser EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/cpp-feather-ini-parser/example/example.cpp)
target_link_libraries(test_cpp_feather_ini_parser PRIVATE cpp-feather-ini-parser)
add_test(test_cpp_feather_ini_parser test_cpp_feather_ini_parser)

if(NOT USE_SYSTEM_BOOST)
    add_library(boost_config IMPORTED INTERFACE)
    set_property(TARGET boost_config PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-config/include>")

    add_library(boost_utility IMPORTED INTERFACE)
    set_property(TARGET boost_utility PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-utility/include>")
    set_property(TARGET boost_utility PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_predef IMPORTED INTERFACE)
    set_property(TARGET boost_predef PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-predef/include>")
    set_property(TARGET boost_predef PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_assert IMPORTED INTERFACE)
    set_property(TARGET boost_assert PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-assert/include>")
    set_property(TARGET boost_assert PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_core IMPORTED INTERFACE)
    set_property(TARGET boost_core PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-core/include>")
    set_property(TARGET boost_core PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_type_traits IMPORTED INTERFACE)
    set_property(TARGET boost_type_traits PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-type_traits/include>")
    set_property(TARGET boost_type_traits PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_iterator IMPORTED INTERFACE)
    set_property(TARGET boost_iterator PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-iterator/include>")
    set_property(TARGET boost_iterator PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_mpl IMPORTED INTERFACE)
    set_property(TARGET boost_mpl PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-mpl/include>")
    set_property(TARGET boost_mpl PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_preprocessor IMPORTED INTERFACE)
    set_property(TARGET boost_preprocessor PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-preprocessor/include>")
    set_property(TARGET boost_preprocessor PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_static_assert IMPORTED INTERFACE)
    set_property(TARGET boost_static_assert PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-static_assert/include>")
    set_property(TARGET boost_static_assert PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_detail IMPORTED INTERFACE)
    set_property(TARGET boost_detail PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-detail/include>")
    set_property(TARGET boost_detail PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_smart_ptr IMPORTED INTERFACE)
    set_property(TARGET boost_smart_ptr PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-smart_ptr/include>")
    set_property(TARGET boost_smart_ptr PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_throw_exception IMPORTED INTERFACE)
    set_property(TARGET boost_throw_exception PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-throw_exception/include>")
    set_property(TARGET boost_throw_exception PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_io IMPORTED INTERFACE)
    set_property(TARGET boost_io PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-io/include>")
    set_property(TARGET boost_io PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_container_hash IMPORTED INTERFACE)
    set_property(TARGET boost_container_hash PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-container_hash/include>")
    set_property(TARGET boost_container_hash PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_functional IMPORTED INTERFACE)
    set_property(TARGET boost_functional PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-functional/include>")
    set_property(TARGET boost_functional PROPERTY INTERFACE_LINK_LIBRARIES "boost_config;boost_container_hash")

    add_library(boost_range IMPORTED INTERFACE)
    set_property(TARGET boost_range PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-range/include>")
    set_property(TARGET boost_range PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

    add_library(boost_integer IMPORTED INTERFACE)
    set_property(TARGET boost_integer PROPERTY INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/boost-integer/include>")
    set_property(TARGET boost_integer PROPERTY INTERFACE_LINK_LIBRARIES "boost_config")

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
    target_link_libraries(boost_filesystem PUBLIC boost_config boost_utility boost_system)

    file(GLOB boost_regex_srcs ${CMAKE_CURRENT_SOURCE_DIR}/boost-regex/src/*.cpp)
    add_library(boost_regex STATIC ${boost_regex_srcs})
    target_include_directories(boost_regex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/boost-regex/include)
    target_link_libraries(boost_regex PUBLIC
        boost_config boost_predef boost_assert boost_throw_exception boost_smart_ptr boost_core boost_mpl
        boost_type_traits boost_static_assert boost_integer boost_preprocessor boost_functional boost_detail
    )
endif()
