# 3.5 is required for imported boost targets
# 3.6 is required for the PkgConfig module's IMPORTED_TARGET library feature
cmake_minimum_required(VERSION 3.6)

# include headers to make CLion happy
file(GLOB HEADERS ${PROJECT_SOURCE_DIR}/include/linuxdeploy/core/*.h)

if(USE_SYSTEM_BOOST)
    set(Boost_USE_STATIC_LIBS ON)
    find_package(Boost REQUIRED COMPONENTS filesystem regex)
    set(BOOST_LIBS Boost::filesystem Boost::regex)
else()
    # use custom built libs
    set(BOOST_LIBS Boost_system Boost_filesystem Boost_regex)
endif()

find_package(Threads)

find_package(LibMagic)

find_package(PkgConfig)
pkg_check_modules(magick++ REQUIRED IMPORTED_TARGET Magick++)

message(STATUS "Generating excludelist")
execute_process(
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/generate-excludelist.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_library(linuxdeploy_core STATIC elf.cpp log.cpp  appdir.cpp desktopfile.cpp ${HEADERS})
target_link_libraries(linuxdeploy_core PUBLIC linuxdeploy_util ${BOOST_LIBS} subprocess cpp-feather-ini-parser PkgConfig::magick++ libmagic_static ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(linuxdeploy_core PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(linuxdeploy_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(linuxdeploy_core PUBLIC -DBOOST_NO_CXX11_SCOPED_ENUMS)

add_executable(linuxdeploy main.cpp)
target_link_libraries(linuxdeploy linuxdeploy_core args)
set_target_properties(linuxdeploy PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
