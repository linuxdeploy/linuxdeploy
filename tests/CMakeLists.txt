# first build dependencies for tests
add_subdirectory(simple_library)
add_subdirectory(simple_executable)

# now include actual tests
add_subdirectory(core)
