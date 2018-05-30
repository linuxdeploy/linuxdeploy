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
