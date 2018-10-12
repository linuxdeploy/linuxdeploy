if(NOT TARGET gtest)
    include(AddGoogleTest)
endif()

add_subdirectory(simple_library)
add_subdirectory(simple_executable)

add_executable(AppDirUnitTests AppDirUnitTests.cpp)
target_link_libraries(AppDirUnitTests PRIVATE linuxdeploy_core)
target_include_directories(AppDirUnitTests PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_compile_definitions(AppDirUnitTests PRIVATE
    -DSIMPLE_LIBRARY_PATH="$<TARGET_FILE:simple_library>"
    -DSIMPLE_EXECUTABLE_PATH="$<TARGET_FILE:simple_executable>"
    )

add_gtest(AppDirUnitTests)
add_dependencies(AppDirUnitTests simple_library simple_executable)
