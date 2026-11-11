add_library(dependency_leaf SHARED dependency_leaf.cpp dependency_leaf.h)
target_include_directories(dependency_leaf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(dependency_leaf_static STATIC dependency_leaf.cpp dependency_leaf.h)
target_include_directories(dependency_leaf_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# replacement for dependency_leaf which lacks the symbol dependency_parent needs, to cause relocation errors
add_library(dependency_leaf_missing_symbol SHARED missing_symbol.cpp)
