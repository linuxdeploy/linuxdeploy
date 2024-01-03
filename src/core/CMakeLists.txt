cmake_minimum_required(VERSION 2.8)

# include headers to make CLion happy
file(GLOB HEADERS ${PROJECT_SOURCE_DIR}/include/linuxdeploy/core/*.h)

find_package(Threads)

find_package(CImg)

if(NOT CImg_FOUND)
    message(STATUS "CImg not found, fetching from GitHub")

    cmake_minimum_required(VERSION 3.14)

    FetchContent_Declare(
        CImg
        GIT_REPOSITORY https://github.com/dtschump/CImg
        GIT_TAG v.2.9.9
    )

    FetchContent_MakeAvailable(CImg)

    # unfortunately there is no easy way to fetch the source dir with MakeAvailable
    # FETCHCONTENT_SOURCE_DIR_CIMG is not set
    # we can apparently predict the path from the base dir, though
    # we'll have to see how reliable this is going to be
    set(CIMG_DIR "${FETCHCONTENT_BASE_DIR}/cimg-src")
    find_package(CImg REQUIRED)
endif()

message(STATUS "Generating excludelist")
execute_process(
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/generate-excludelist.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_subdirectory(copyright)

add_library(linuxdeploy_core STATIC elf_file.cpp appdir.cpp ${HEADERS} appdir_root_setup.cpp)
target_link_libraries(linuxdeploy_core PUBLIC
    linuxdeploy_plugin linuxdeploy_log linuxdeploy_util linuxdeploy_desktopfile_static
    CImg ${CMAKE_THREAD_LIBS_INIT}
)
target_link_libraries(linuxdeploy_core PRIVATE linuxdeploy_core_copyright)
target_include_directories(linuxdeploy_core PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(linuxdeploy_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
