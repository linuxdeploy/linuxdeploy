add_library(linuxdeploy_util STATIC magicwrapper.cpp magicwrapper.h util.h)
target_include_directories(linuxdeploy_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
