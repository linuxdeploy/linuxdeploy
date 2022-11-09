
# Include this file in a top-level CMakeLists to build several CMake
# subprojects (which may depend on each other).
#
# When included, it will automatically parse a .gitsubprojects file if one is
# present in the same CMake source directory. The .gitsubprojects file
# contains lines in the form:
#   "git_subproject(<project> <giturl> <gittag>)"
#
# All the subprojects will be cloned and configured during the CMake configure
# step thanks to the 'git_subproject(project giturl gittag)' macro
# (also usable separately).
# The latter relies on the add_subproject(name) function to add projects as
# sub directories. See also: cmake command 'add_subdirectory'.
#
# The following targets are created by SubProject.cmake:
# - A '${PROJECT_NAME}-update-gitsubprojects' target to update the <gittag> of
#   all the .gitsubprojects entries to their latest respective origin/master ref
# - A generic 'update' target to execute 'update-git-subrojects' recursively
#
# To be compatible with the SubProject feature, (sub)projects might need to
# adapt their CMake scripts in the following way:
# - CMAKE_BINARY_DIR should be changed to PROJECT_BINARY_DIR
# - CMAKE_SOURCE_DIR should be changed to PROJECT_SOURCE_DIR
#
# They must also be locatable by CMake's find_package(name), which can be
# achieved in any of the following ways:
# - include(CommonPackageConfig) at the end of the top-level CMakeLists.txt
# - include(CommonCPack), which indirectly includes CommonPackageConfig
# - Provide a compatible Find<Name>.cmake script (not recommended)
#
# If the project needs to do anything special when configured as a sub project
# then it can check the variable ${PROJECT_NAME}_IS_SUBPROJECT.
#
# SubProject.cmake respects the following variables:
# - CLONE_SUBPROJECTS: when set, subprojects are git-cloned if missing.
# - DISABLE_SUBPROJECTS: when set, does not load sub projects. Useful for
#   example for continuous integration builds
# - SUBPROJECT_${name}: If set to OFF, the subproject is not added.
# - COMMON_SOURCE_DIR: When set, the source code of subprojects will be
#   downloaded in this path instead of CMAKE_SOURCE_DIR.
# - ${PROJECT_NAME}_IS_METAPROJECT: Enable CommonGraph compatibility for a
#   top-level project which does not use common_find_package().
# A sample project can be found at https://github.com/Eyescale/Collage.git
#
# How to create a dependency graph:
#  cmake --graphviz=graph.dot
#  tred graph.dot > graph2.dot
#  neato -Goverlap=prism -Goutputorder=edgesfirst graph2.dot -Tpdf -o graph.pdf
#
# Output Variables:
# - SUBPROJECT_PATHS: list of paths to subprojects, useful for exclusion lists

include(${CMAKE_CURRENT_LIST_DIR}/GitExternal.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/CommonGraph.cmake)

get_property(SUBPROJECT_DONE GLOBAL PROPERTY GIT_SUBPROJECT_DONE_${PROJECT_NAME})
if(SUBPROJECT_DONE)
  return()
endif()
set_property(GLOBAL PROPERTY GIT_SUBPROJECT_DONE_${PROJECT_NAME} ON)

set(COMMON_SOURCE_DIR "${CMAKE_SOURCE_DIR}" CACHE PATH
  "Location of common directory of all sources")
set(__common_source_dir ${COMMON_SOURCE_DIR})
get_filename_component(__common_source_dir ${__common_source_dir} ABSOLUTE)
file(MAKE_DIRECTORY ${__common_source_dir})

function(add_subproject name)
  string(TOUPPER ${name} NAME)
  if(CMAKE_MODULE_PATH)
    # We're adding a sub project here: Remove parent's CMake
    # directories so they don't take precendence over the sub project
    # directories. Change is scoped to this function.
    list(REMOVE_ITEM CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake)
  endif()

  list(LENGTH ARGN argc)
  if(argc GREATER 0)
    list(GET ARGN 0 path)
  else()
    set(path ${__common_source_dir}/${name})
  endif ()

  if(NOT EXISTS "${path}/")
    message(FATAL_ERROR
      "Subproject ${name} not found in ${path}, do:\n"
      "cmake -DCLONE_SUBPROJECTS=ON\n"
      "to git-clone it automatically.")
  endif()

  # allow exclusion of subproject via set(SUBPROJECT_${name} OFF)
  if(DEFINED SUBPROJECT_${name} AND NOT SUBPROJECT_${name})
    option(SUBPROJECT_${name} "Build ${name} " OFF)
  else()
    option(SUBPROJECT_${name} "Build ${name} " ON)
  endif()
  if(NOT SUBPROJECT_${name})
    return()
  endif()

  # Hint for the sub project, in case it needs to do anything special when
  # configured as a sub project
  set(${name}_IS_SUBPROJECT ON)

  # set ${PROJECT}_DIR to the location of the new build dir for the project
  if(NOT ${name}_DIR)
    set(${name}_DIR "${CMAKE_BINARY_DIR}/${name}" CACHE PATH
      "Location of ${name} project" FORCE)
  endif()

  # add the source sub directory to our build and set the binary dir
  # to the build tree
  add_subdirectory("${path}" "${CMAKE_BINARY_DIR}/${name}")
  set(${name}_IS_SUBPROJECT ON PARENT_SCOPE)

  # Mark globally that we've already used name as a sub project
  set_property(GLOBAL PROPERTY ${name}_IS_SUBPROJECT ON)
endfunction()

macro(git_subproject name url tag)
  if(DISABLE_SUBPROJECTS)
    return()
  endif()

  # add graph dependency for meta-projects with no common_find_package() calls
  if(${PROJECT_NAME}_IS_METAPROJECT)
    common_graph_dep(${PROJECT_NAME} ${name} SOURCE TOPLEVEL REQUIRED)
  endif()

  string(TOUPPER ${name} NAME)
  if(NOT ${NAME}_FOUND AND NOT ${name}_FOUND)
    get_property(__included GLOBAL PROPERTY ${name}_IS_SUBPROJECT)
    if(NOT __included)
      if(NOT EXISTS ${__common_source_dir}/${name})
        # Always try first using Config mode, then Module mode.
        find_package(${name} QUIET CONFIG)
        if(NOT ${NAME}_FOUND AND NOT ${name}_FOUND)
          find_package(${name} QUIET MODULE)
        endif()
      endif()
      if((NOT ${NAME}_FOUND AND NOT ${name}_FOUND) OR
          ${NAME}_FOUND_SUBPROJECT)
        # not found externally, add as sub project
        if(CLONE_SUBPROJECTS)
          git_external(${__common_source_dir}/${name} ${url} ${tag})
        endif()
        add_subproject(${name})
      endif()
    endif()
  endif()
  get_property(__included GLOBAL PROPERTY ${name}_IS_SUBPROJECT)
  if(__included)
    list(APPEND __subprojects "${name} ${url} ${tag}")
  endif()
endmacro()

# Interpret .gitsubprojects
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.gitsubprojects")
  set(__subprojects) # appended on each git_subproject invocation
  include(.gitsubprojects)
  if(__subprojects)
    get_property(__subproject_paths GLOBAL PROPERTY SUBPROJECT_PATHS)
    set(GIT_SUBPROJECTS_SCRIPT
      "${CMAKE_CURRENT_BINARY_DIR}/UpdateSubprojects.cmake")
    file(WRITE "${GIT_SUBPROJECTS_SCRIPT}"
      "file(READ .gitsubprojects GIT_SUBPROJECTS_FILE)\n")
    foreach(__subproject ${__subprojects})
      string(REPLACE " " ";" __subproject_list ${__subproject})
      list(GET __subproject_list 0 __subproject_name)
      list(GET __subproject_list 1 __subproject_repo)
      list(GET __subproject_list 2 __subproject_oldref)
      list(APPEND SUBPROJECTS ${__subproject_name})
      set(__subproject_dir "${__common_source_dir}/${__subproject_name}")
      file(APPEND "${GIT_SUBPROJECTS_SCRIPT}"
        "execute_process(COMMAND \"${GIT_EXECUTABLE}\" fetch origin -q\n"
        "  WORKING_DIRECTORY ${__subproject_dir})\n"
        "execute_process(COMMAND \n"
        "  \"${GIT_EXECUTABLE}\" show-ref --hash=7 refs/remotes/origin/master\n"
        "  OUTPUT_VARIABLE newref OUTPUT_STRIP_TRAILING_WHITESPACE\n"
        "  WORKING_DIRECTORY \"${__subproject_dir}\")\n"
        "if(newref)\n"
        "  string(REPLACE \"${__subproject_name} ${__subproject_repo} ${__subproject_oldref}\"\n"
        "                 \"${__subproject_name} ${__subproject_repo} \${newref}\"\n"
        "                 GIT_SUBPROJECTS_FILE \${GIT_SUBPROJECTS_FILE})\n"
        "endif()\n")
        list(APPEND __subproject_paths ${__subproject_dir})
    endforeach()
    file(APPEND "${GIT_SUBPROJECTS_SCRIPT}"
      "file(WRITE .gitsubprojects \${GIT_SUBPROJECTS_FILE})\n")

    list(REMOVE_DUPLICATES __subproject_paths)
    set_property(GLOBAL PROPERTY SUBPROJECT_PATHS ${__subproject_paths})

    add_custom_target(${PROJECT_NAME}-update-gitsubprojects
      COMMAND ${CMAKE_COMMAND} -P ${GIT_SUBPROJECTS_SCRIPT}
      COMMENT "Update ${PROJECT_NAME}/.gitsubprojects"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    set_target_properties(${PROJECT_NAME}-update-gitsubprojects PROPERTIES
      EXCLUDE_FROM_DEFAULT_BUILD ON FOLDER ${PROJECT_NAME}/zzphony)

    if(NOT TARGET update)
      add_custom_target(update)
      set_target_properties(update PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON)
    endif()
    add_dependencies(update ${PROJECT_NAME}-update-gitsubprojects)

    if(${PROJECT_NAME}_IS_METAPROJECT)
      common_graph(${PROJECT_NAME})
    endif()
  endif()
endif()
