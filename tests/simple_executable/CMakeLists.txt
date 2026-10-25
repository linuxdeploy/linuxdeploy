add_executable(simple_executable simple_executable.cpp)
target_link_libraries(simple_executable simple_library pthread)

add_executable(simple_executable_static simple_executable.cpp)
target_link_libraries(simple_executable_static simple_library_static pthread)
# keep this fixture a classic static executable, GCC may link -static as static-pie
# (e.g., on Alpine), which the fixture below covers
target_link_options(simple_executable_static PUBLIC
    -static
    -static-libgcc
    -static-libstdc++
    $<$<CXX_COMPILER_ID:GNU>:-no-pie>
)

# static-pie executables contain a PT_DYNAMIC segment, but must still be treated
# as statically linked, this fixture is optional, check for toolchain support:
# - the toolchain needs a static libc built for static-pie
# - clang only warns about unsupported -static-pie (e.g., on FreeBSD) and links a
#   dynamic PIE, so add -Werror to make it into an error for the test
# - older CMake versions ignore CMAKE_REQUIRED_LINK_OPTIONS, so the check would
#   always succeed
include(CheckCSourceCompiles)
if(NOT CMAKE_VERSION VERSION_LESS 3.14)
    set(CMAKE_REQUIRED_FLAGS "-fPIE")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-static-pie;-Werror")
    check_c_source_compiles("int main(void) { return 0; }" HAVE_STATIC_PIE)
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
endif()

if(HAVE_STATIC_PIE)
    add_executable(simple_executable_static_pie static_pie_executable.c)
    target_compile_options(simple_executable_static_pie PRIVATE -fPIE)
    target_link_options(simple_executable_static_pie PUBLIC -static-pie)
endif()

# PIEs which need other libraries but have no program interpreter must still be
# treated as dynamically linked, this fixture is optional, check for toolchain support:
# - linker needs to support --no-dynamic-linker
set(CMAKE_REQUIRED_FLAGS "-fPIE -pie -Wl,--no-dynamic-linker")
check_c_source_compiles("int main(void) { return 0; }" HAVE_NO_DYNAMIC_LINKER)
unset(CMAKE_REQUIRED_FLAGS)

if(HAVE_NO_DYNAMIC_LINKER)
    add_executable(simple_executable_no_interpreter_pie no_interpreter_pie_executable.c)
    target_link_libraries(simple_executable_no_interpreter_pie simple_library_no_dependencies)
    target_compile_options(simple_executable_no_interpreter_pie PRIVATE -fPIE)
    # the coverage runtime needs the C library, so do not instrument the fixture
    if(ENABLE_COVERAGE)
        target_compile_options(simple_executable_no_interpreter_pie PRIVATE -fprofile-exclude-files=.*)
    endif()
    # see simple_library_no_dependencies for the warning option
    target_link_options(simple_executable_no_interpreter_pie PUBLIC
        -pie
        -nostdlib
        -Wl,--no-dynamic-linker
        $<$<C_COMPILER_ID:Clang>:-Wno-unused-command-line-argument>
    )
endif()
