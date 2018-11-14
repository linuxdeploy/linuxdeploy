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
)

# register in CTest
add_test(test_linuxdeploy test_linuxdeploy)

# make sure library and executable are built before test_appdir
add_dependencies(test_linuxdeploy simple_library simple_executable)


add_executable(test_desktopfilereader test_desktopfilereader.cpp)
target_link_libraries(test_desktopfilereader PRIVATE linuxdeploy_core gtest gtest_main)

add_test(test_desktopfilereader test_desktopfilereader)


add_executable(test_desktopfileentry test_desktopfileentry.cpp)
target_link_libraries(test_desktopfileentry PRIVATE linuxdeploy_core gtest gtest_main)

add_test(test_desktopfileentry test_desktopfileentry)


add_executable(test_desktopfilewriter test_desktopfilewriter.cpp)
target_link_libraries(test_desktopfilewriter PRIVATE linuxdeploy_core gtest gtest_main)

add_test(test_desktopfilewriter test_desktopfilewriter)


add_executable(test_desktopfile test_desktopfile.cpp)
target_link_libraries(test_desktopfile PRIVATE linuxdeploy_core gtest gtest_main)
set_property(TARGET test_desktopfile
    PROPERTY COMPILE_FLAGS "-DDESKTOP_FILE_PATH=\\\"${CMAKE_CURRENT_SOURCE_DIR}/../data/simple_app.desktop\\\""
)

add_test(test_desktopfile test_desktopfile)
