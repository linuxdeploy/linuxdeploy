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

    file(GLOB boost_system_srcs ${CMAKE_CURRENT_SOURCE_DIR}/boost-system/src/*.cpp)
    add_library(boost_system STATIC ${boost_system_srcs})
    target_include_directories(boost_system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/boost-system/include)
    target_link_libraries(boost_system INTERFACE boost_config)

    file(GLOB boost_filesystem_srcs ${CMAKE_CURRENT_SOURCE_DIR}/boost-filesystem/src/*.cpp)
    add_library(boost_filesystem STATIC ${boost_filesystem_srcs})
    target_include_directories(boost_filesystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/boost-filesystem/include)
    target_link_libraries(boost_filesystem INTERFACE boost_config)

    file(GLOB boost_regex_srcs ${CMAKE_CURRENT_SOURCE_DIR}/boost-regex/src/*.cpp)
    add_library(boost_regex STATIC ${boost_regex_srcs})
    target_include_directories(boost_regex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/boost-regex/include)
    target_link_libraries(boost_regex INTERFACE boost_config)
endif()
