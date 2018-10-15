if(NOT TARGET gtest)
    include(AddGoogleTest)
endif()

add_executable(AppDirUnitTests AppDirUnitTests.cpp)
target_link_libraries(AppDirUnitTests PRIVATE linuxdeploy_core)
target_include_directories(AppDirUnitTests PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_gtest(AppDirUnitTests)
