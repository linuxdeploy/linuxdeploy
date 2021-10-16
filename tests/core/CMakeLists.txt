add_executable(test_appdir test_appdir.cpp)
target_link_libraries(test_appdir PRIVATE linuxdeploy_core gtest)
target_include_directories(test_appdir PRIVATE ${PROJECT_SOURCE_DIR}/include)

# calculate paths to resources using CMake and hardcode them in the test binary
target_compile_definitions(test_appdir PRIVATE
    -DSIMPLE_LIBRARY_PATH="$<TARGET_FILE:simple_library>"
    -DSIMPLE_EXECUTABLE_PATH="$<TARGET_FILE:simple_executable>"
    -DSIMPLE_DESKTOP_ENTRY_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/simple_app.desktop"
    -DSIMPLE_ICON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/simple_icon.svg"
    -DSIMPLE_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/simple_file.txt"
    -DREADONLY_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/readonly-file.txt"
)

# register in CTest
ld_add_test(test_appdir)

# make sure library and executable are built before test_appdir
add_dependencies(test_appdir simple_library simple_executable)


add_executable(test_linuxdeploy test_linuxdeploy.cpp ../../src/core.cpp)
target_link_libraries(test_linuxdeploy PRIVATE linuxdeploy_core args gtest gtest_main)
target_include_directories(test_linuxdeploy PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

# calculate paths to resources using CMake and hardcode them in the test binary
target_compile_definitions(test_linuxdeploy PRIVATE
    -DSIMPLE_LIBRARY_PATH="$<TARGET_FILE:simple_library>"
    -DSIMPLE_EXECUTABLE_PATH="$<TARGET_FILE:simple_executable>"
    -DSIMPLE_DESKTOP_ENTRY_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/simple_app.desktop"
    -DSIMPLE_ICON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/simple_icon.svg"
    -DSIMPLE_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/simple_file.txt"
    -DREADONLY_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/readonly-file.txt"
)

# register in CTest
ld_add_test(test_linuxdeploy)
add_executable(test_elf_file test_elf_file.cpp ../../src/core.cpp)
target_link_libraries(test_elf_file PRIVATE linuxdeploy_core args gtest gtest_main)
target_include_directories(test_elf_file PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

# calculate paths to resources using CMake and hardcode them in the test binary
target_compile_definitions(test_elf_file PRIVATE
    -DSIMPLE_LIBRARY_PATH="$<TARGET_FILE:simple_library>"
    -DSIMPLE_LIBRARY_DEBUG_PATH="$<TARGET_FILE:simple_library>.debug"
    -DSIMPLE_LIBRARY_STRIPPED_PATH="$<TARGET_FILE:simple_library>.stripped"
    -DSIMPLE_EXECUTABLE_PATH="$<TARGET_FILE:simple_executable>"
    -DSIMPLE_EXECUTABLE_STATIC_PATH="$<TARGET_FILE:simple_executable_static>"
    -DREADONLY_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/readonly-file.txt"
)

# make sure library and executable are built before test_appdir
add_dependencies(test_elf_file
    simple_executable
    simple_executable_static
    simple_library
    simple_library_stripped
    simple_library_debug
)

# register in CTest
ld_add_test(test_elf_file)

