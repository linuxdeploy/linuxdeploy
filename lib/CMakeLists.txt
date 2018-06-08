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
    file(GLOB boost_system_srcs ${CMAKE_CURRENT_SOURCE_DIR}/boost-system/src/*.cpp)
    add_library(Boost_system STATIC ${boost_system_srcs})
    target_include_directories(Boost_system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/boost-system/include)

    file(GLOB boost_filesystem_srcs ${CMAKE_CURRENT_SOURCE_DIR}/boost-filesystem/src/*.cpp)
    add_library(Boost_filesystem STATIC ${boost_filesystem_srcs})
    target_include_directories(Boost_filesystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/boost-filesystem/include)

    file(GLOB boost_regex_srcs ${CMAKE_CURRENT_SOURCE_DIR}/boost-regex/src/*.cpp)
    add_library(Boost_regex STATIC ${boost_regex_srcs})
    target_include_directories(Boost_regex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/boost-regex/include)
endif()
