# build a single test binary
add_executable(test_desktopfile
    test_desktopfile.cpp
    test_desktopfileentry.cpp
    test_desktopfilereader.cpp
    test_desktopfilewriter.cpp
    test_desktopfile_conformance.cpp
    main.cpp
)

target_link_libraries(test_desktopfile PRIVATE linuxdeploy_core_desktopfile gtest)

set_property(TARGET test_desktopfile
    PROPERTY COMPILE_FLAGS "-DDESKTOP_FILE_PATH=\\\"${TEST_DATA_DIR}/simple_app.desktop\\\""
)

add_test(test_desktopfile test_desktopfile)
