add_library(simple_library SHARED simple_library.cpp simple_library.h)
target_link_libraries(simple_library PUBLIC CImg)
target_include_directories(simple_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# generate a debug symbols file
add_custom_command(
    OUTPUT libsimple_library.so.debug
    COMMAND "${CMAKE_OBJCOPY}" --only-keep-debug libsimple_library.so libsimple_library.so.debug
    DEPENDS simple_library
    VERBATIM
)
add_custom_target(simple_library_debug ALL DEPENDS libsimple_library.so.debug)

# generate a library whose debug symbols have been stripped, but which contains a GNU debug link to the symbols file
add_custom_command(
    OUTPUT libsimple_library.so.stripped
    COMMAND "${CMAKE_OBJCOPY}" --strip-debug libsimple_library.so libsimple_library.so.stripped
    COMMAND "${CMAKE_OBJCOPY}" --add-gnu-debuglink=libsimple_library.so.debug libsimple_library.so.stripped
    DEPENDS simple_library_debug
    VERBATIM
)
add_custom_target(simple_library_stripped ALL DEPENDS libsimple_library.so.stripped)

# shared libraries which need no other libraries must still be considered dynamically linked
add_library(simple_library_no_dependencies SHARED no_dependencies.c)
# without the runtime libraries, clang warns about runtime options a toolchain may pass in $LDFLAGS (e.g., -rtlib)
target_link_options(simple_library_no_dependencies PRIVATE
    -nostdlib
    $<$<C_COMPILER_ID:Clang>:-Wno-unused-command-line-argument>
)
# the coverage runtime needs the C library, so do not instrument the fixture
if(ENABLE_COVERAGE)
    target_compile_options(simple_library_no_dependencies PRIVATE -fprofile-exclude-files=.*)
endif()

# add a static version, too
add_library(simple_library_static STATIC simple_library.cpp simple_library.h)
target_link_libraries(simple_library_static PUBLIC CImg)
target_include_directories(simple_library_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
