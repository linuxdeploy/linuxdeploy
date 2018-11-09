cmake_minimum_required(VERSION 3.2)

project(linuxdeploy C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")

set(USE_SYSTEM_BOOST OFF CACHE BOOL "Set to ON to use system boost libraries instead of building up to date boost libraries from source")
set(USE_SYSTEM_CIMG ON CACHE BOOL "Set to OFF to use CImg library bundled in lib directory")

set(ENABLE_COVERAGE OFF CACHE BOOL "Enable coverage measurements")
if(ENABLE_COVERAGE)
    include(CodeCoverage)
    message(WARNING "Enabling code coverage measurements -> disables optimizations and embeds debug information!")

    append_coverage_compiler_flags()

    set(COVERAGE_GCOVR_EXCLUDES ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR})
    set(command ctest -V -j ${PROCESSOR_COUNT})
    setup_target_for_coverage_gcovr_html(NAME coverage EXECUTABLE "${command}")
    setup_target_for_coverage_gcovr_xml(NAME coverage_xml EXECUTABLE "${command}")
    setup_target_for_coverage_gcovr_text(NAME coverage_text EXECUTABLE "${command}")
    setup_target_for_coverage_lcov(NAME coverage_lcov EXECUTABLE "${command}")
endif()

add_subdirectory(lib)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
