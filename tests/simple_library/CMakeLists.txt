add_library(simple_library SHARED simple_library.cpp simple_library.h)
target_link_libraries(simple_library PUBLIC CImg)
target_include_directories(simple_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
