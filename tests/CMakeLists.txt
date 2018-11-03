# first build dependencies for tests
add_subdirectory(simple_library)
add_subdirectory(simple_executable)

# now include actual tests
add_subdirectory(core)

add_test(NAME build_minial_appimage
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_minimal_appimage_with_custom_apprun.sh
        $<TARGET_FILE:linuxdeploy>
        $<TARGET_FILE:simple_executable>
        "${CMAKE_CURRENT_SOURCE_DIR}/data/simple_app.desktop"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/simple_icon.svg"
)
SET_TESTS_PROPERTIES(build_minial_appimage PROPERTIES DEPENDS "linuxdeploy simple_executable")
