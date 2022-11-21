add_library(simple_library SHARED simple_library.cpp simple_library.h)
target_link_libraries(simple_library PUBLIC CImg)
target_include_directories(simple_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# generate a debug symbols file
add_custom_command(
    OUTPUT libsimple_library.so.debug
    COMMAND objcopy --only-keep-debug libsimple_library.so libsimple_library.so.debug
    DEPENDS simple_library
    VERBATIM
)
add_custom_target(simple_library_debug ALL DEPENDS libsimple_library.so.debug)

# generate a library whose debug symbols have been stripped, but which contains a GNU debug link to the symbols file
add_custom_command(
    OUTPUT libsimple_library.so.stripped
    COMMAND objcopy --strip-debug libsimple_library.so libsimple_library.so.stripped
    COMMAND objcopy --add-gnu-debuglink=libsimple_library.so.debug libsimple_library.so.stripped
    DEPENDS simple_library_debug
    VERBATIM
)
add_custom_target(simple_library_stripped ALL DEPENDS libsimple_library.so.stripped)

# add a static version, too
add_library(simple_library_static STATIC simple_library.cpp simple_library.h)
target_link_libraries(simple_library_static PUBLIC CImg)
target_include_directories(simple_library_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
