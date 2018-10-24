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
)

# register in CTest
add_test(test_appdir test_appdir)

# make sure library and executable are built before test_appdir
add_dependencies(test_appdir simple_library simple_executable)
