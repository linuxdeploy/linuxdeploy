add_library(args INTERFACE)
target_sources(args INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/args/args.hxx)
target_include_directories(args INTERFACE args)

add_subdirectory(linuxdeploy-desktopfile EXCLUDE_FROM_ALL)
