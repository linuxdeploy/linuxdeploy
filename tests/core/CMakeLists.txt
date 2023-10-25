add_library(test_util OBJECT test_util.cpp)

function(ld_core_add_test_executable NAME)
    add_executable(${ARGV})
    target_link_libraries(${NAME} PRIVATE linuxdeploy_core gtest test_util)
    target_include_directories(${NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

    # calculate paths to resources using CMake and hardcode them in the test binary
    target_compile_definitions(${NAME} PRIVATE
        -DSIMPLE_LIBRARY_PATH="$<TARGET_FILE:simple_library>"
        -DSIMPLE_LIBRARY_DEBUG_PATH="$<TARGET_FILE:simple_library>.debug"
        -DSIMPLE_LIBRARY_STRIPPED_PATH="$<TARGET_FILE:simple_library>.stripped"

        -DSIMPLE_EXECUTABLE_PATH="$<TARGET_FILE:simple_executable>"
        -DSIMPLE_EXECUTABLE_STATIC_PATH="$<TARGET_FILE:simple_executable_static>"

        -DSIMPLE_DESKTOP_ENTRY_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/simple_app.desktop"
        -DSIMPLE_ICON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/simple_icon.png"
        -DSIMPLE_ICON_PATH2="${CMAKE_CURRENT_SOURCE_DIR}/../data/simple_icon.svg"
        -DSIMPLE_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/simple_file.txt"
        -DREADONLY_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../data/readonly-file.txt"
    )

    # make sure library and executable are built before test_appdir
    add_dependencies(${NAME}
        simple_executable
        simple_executable_static
        simple_library
        simple_library_stripped
        simple_library_debug
    )
endfunction()

ld_core_add_test_executable(test_appdir test_appdir.cpp)
# register in CTest
ld_add_test(test_appdir)

ld_core_add_test_executable(test_linuxdeploy test_linuxdeploy.cpp ../../src/core.cpp)
# additional dependencies
target_link_libraries(test_linuxdeploy PRIVATE gtest_main)
target_include_directories(test_linuxdeploy PRIVATE ${PROJECT_SOURCE_DIR}/src)
# register in CTest
ld_add_test(test_linuxdeploy)

ld_core_add_test_executable(test_elf_file test_elf_file.cpp ../../src/core.cpp)
target_link_libraries(test_elf_file PRIVATE gtest_main gmock)
target_include_directories(test_elf_file PRIVATE ${PROJECT_SOURCE_DIR}/src)
# register in CTest
ld_add_test(test_elf_file)

