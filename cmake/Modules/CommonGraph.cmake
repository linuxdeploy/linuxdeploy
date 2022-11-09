# Copyright (c) 2017 Stefan.Eilemann@epfl.ch
#                    Raphael.Dumusc@epfl.ch
#
# Provides functions to generate .png dependency graph images using graphviz:
#   common_graph_dep(From To Required Source):
#     Write a dependency From->To into global properties, called by
#     common_find_package() and add_subproject().
#   common_graph(Name):
#     Write .dot from the global properties and add Name-graph target, called by
#     common_find_package_post().
#
# CMake options:
# * COMMON_GRAPH_SHOW_EXTERNAL include external dependencies in graphs.
# * COMMON_GRAPH_SHOW_OPTIONAL include optional dependencies in graphs.
# * COMMON_GRAPH_SHOW_NOTFOUND include missing dependencies in graphs.
#
# Targets generated:
# * graphs: generate .png graphs for all (sub)projects.
# * ${PROJECT_NAME}-graph generate a .png graph for the project.

include(CommonTarget)
if(COMMON_GRAPH_DONE)
  return()
endif()
set(COMMON_GRAPH_DONE ON)

option(COMMON_GRAPH_SHOW_EXTERNAL "Include external dependencies in graphs" ON)
option(COMMON_GRAPH_SHOW_OPTIONAL "Include optional dependencies in graphs" ON)
option(COMMON_GRAPH_SHOW_NOTFOUND "Include missing dependencies in graphs" ON)

find_program(DOT_EXECUTABLE dot)
find_program(TRED_EXECUTABLE tred)

function(common_graph_dep From To)
  # Parse function arguments
  set(__options SOURCE REQUIRED NOTFOUND TOPLEVEL)
  set(__oneValueArgs)
  set(__multiValueArgs)
  cmake_parse_arguments(__dep "${__options}" "${__oneValueArgs}" "${__multiValueArgs}" ${ARGN})

  if(NOT __dep_SOURCE AND NOT COMMON_GRAPH_SHOW_EXTERNAL)
    return()
  endif()
  if(NOT __dep_REQUIRED AND NOT COMMON_GRAPH_SHOW_OPTIONAL)
    return()
  endif()
  if(__dep_NOTFOUND AND NOT COMMON_GRAPH_SHOW_NOTFOUND)
    return()
  endif()

  string(REPLACE "-" "_" Title ${From})
  string(REPLACE "-" "_" Dep ${To})
  # prevent syntax error in tred, e.g. Magick++ -> MagickPP
  string(REPLACE "+" "P" Title ${Title})
  string(REPLACE "+" "P" Dep ${Dep})

  if(__dep_SOURCE)
    set(_style "style=bold")
  elseif(__dep_REQUIRED)
    set(_style "style=solid")
  elseif(__dep_NOTFOUND)
    set(_style "style=dotted")
  else()
    set(_style "style=dashed")
  endif()

  if(__dep_NOTFOUND)
    set(_linestyle "style=dotted")
  elseif(__dep_SOURCE AND __dep_REQUIRED)
    set(_linestyle "style=bold")
  elseif(__dep_REQUIRED)
    set(_linestyle "style=solid")
  else()
    set(_linestyle "style=dashed")
  endif()

  if(__dep_TOPLEVEL)
    set_property(GLOBAL APPEND_STRING PROPERTY ${From}_COMMON_GRAPH
      "${Dep} [${_style}, label=\"${To}\"]\n")
  else()
    set_property(GLOBAL APPEND_STRING PROPERTY ${From}_COMMON_GRAPH
      "${Title} [style=bold, label=\"${From}\"]\n"
      "${Dep} [${_style}, label=\"${To}\"]\n"
      "\"${Dep}\" -> \"${Title}\" [${_linestyle}]\n")
  endif()
  set_property(GLOBAL APPEND PROPERTY ${From}_COMMON_GRAPH_DEPENDS ${To})
endfunction()

function(common_graph Name)
  if(NOT DOT_EXECUTABLE OR NOT TRED_EXECUTABLE)
    return()
  endif()

  # collect graph recursively for all dependencies of Name
  get_property(_graph GLOBAL PROPERTY ${Name}_COMMON_GRAPH)
  get_property(_dependencies GLOBAL PROPERTY ${Name}_COMMON_GRAPH_DEPENDS)
  list(LENGTH _dependencies nDepends)

  set(_loop_count 0)
  while(nDepends)
    list(GET _dependencies 0 dependency)
    list(REMOVE_AT _dependencies 0)

    get_property(_dep_graph GLOBAL PROPERTY ${dependency}_COMMON_GRAPH)
    set(_graph "${_dep_graph} ${_graph}")

    get_property(_dep_depends GLOBAL PROPERTY
      ${dependency}_COMMON_GRAPH_DEPENDS)
    foreach(dep ${_dep_depends})
      list(FIND _dependencies ${dep} _found)
      if(_found EQUAL "-1")
        list(APPEND _dependencies ${dep})
      endif()
    endforeach()
    list(LENGTH _dependencies nDepends)

    # prevent infinite looping in the case of circular dependencies
    MATH(EXPR _loop_count "${_loop_count}+1")
    if(_loop_count GREATER 1000)
      message(WARNING "CommonGraph detected a probable circular dependency")
      return()
    endif()
  endwhile()

  set(_dot_file ${CMAKE_CURRENT_BINARY_DIR}/${Name}.dot)
  file(GENERATE OUTPUT ${_dot_file}
    CONTENT "strict digraph G { rankdir=\"RL\"; ${_graph} }")

  set(_tred_dot_file ${CMAKE_CURRENT_BINARY_DIR}/${Name}_tred.dot)
  add_custom_command(OUTPUT ${_tred_dot_file}
    COMMAND ${TRED_EXECUTABLE} ${_dot_file} > ${_tred_dot_file}
    DEPENDS ${_dot_file})

  set(_image_folder ${PROJECT_BINARY_DIR}/doc/images)
  set(_image_file ${_image_folder}/${Name}.png)
  add_custom_command(OUTPUT ${_image_file}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_image_folder}
    COMMAND ${DOT_EXECUTABLE} -o ${_image_file} -Tpng ${_tred_dot_file}
    DEPENDS ${_tred_dot_file})

  add_custom_target(${Name}-graph DEPENDS ${_image_file})
  set_target_properties(${Name}-graph PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON
    FOLDER ${Name}/doxygen)
  common_target(graphs doxygen)
  add_dependencies(graphs ${Name}-graph)
endfunction()
