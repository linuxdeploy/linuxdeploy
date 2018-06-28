# 3.5 is required for imported boost targets
# 3.6 is required for the PkgConfig module's IMPORTED_TARGET library feature
cmake_minimum_required(VERSION 3.6)

# include headers to make CLion happy
file(GLOB HEADERS ${PROJECT_SOURCE_DIR}/include/linuxdeploy/core/*.h)

find_package(Threads)

find_package(LibMagic REQUIRED)

find_package(CImg REQUIRED)

message(STATUS "Generating excludelist")
execute_process(
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/generate-excludelist.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_library(linuxdeploy_core STATIC elf.cpp log.cpp  appdir.cpp desktopfile.cpp ${HEADERS})
target_link_libraries(linuxdeploy_core PUBLIC linuxdeploy_plugin linuxdeploy_util ${BOOST_LIBS} subprocess cpp-feather-ini-parser CImg libmagic_static ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(linuxdeploy_core PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(linuxdeploy_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(linuxdeploy_core PUBLIC -DBOOST_NO_CXX11_SCOPED_ENUMS)
