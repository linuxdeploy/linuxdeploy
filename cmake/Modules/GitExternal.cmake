# Configures an external git repository
#
# Provides function
#    git_external(<directory> <giturl> <gittag> [VERBOSE]
#      [RESET <files>])
#  which will check out directory in CMAKE_SOURCE_DIR (if relative)
#  or in the given absolute path using the given repository and tag
#  (commit-ish).
#
# Options which can be supplied to the function:
#  VERBOSE, when present, this option tells the function to output
#    information about what operations are being performed by git on
#    the repo.
#  OPTIONAL, when present, this option makes this operation optional.
#    The function will output a warning and return if the repo could not be
#    cloned.
#
# Targets:
#  * <directory>-rebase: fetches latest updates and rebases the given external
#    git repository onto it.
#  * rebase: Rebases all git externals, including sub projects
#
# Options (global) which control behaviour:
#  COMMON_GIT_EXTERNAL_VERBOSE
#    This is a global option which has the same effect as the VERBOSE option,
#    with the difference that output information will be produced for all
#    external repos when set.
#  GIT_EXTERNAL_TAG
#    If set, git external tags referring to a SHA1 (not a branch) will be
#    overwritten by this value.
#
# CMake or environment variables:
#  GITHUB_USER
#    If set, a remote called 'user' is set up for github repositories, pointing
#    to git@github.com:<user>/<project>. Also, this remote is used by default
#    for 'git push'.


if(NOT GIT_FOUND)
  find_package(Git QUIET)
endif()
if(NOT GIT_EXECUTABLE)
  return()
endif()

include(CMakeParseArguments)
option(COMMON_GIT_EXTERNAL_VERBOSE "Print git commands as they are executed" OFF)

if(NOT GITHUB_USER AND DEFINED ENV{GITHUB_USER})
  set(GITHUB_USER $ENV{GITHUB_USER} CACHE STRING
    "Github user name used to setup remote for 'user' forks")
endif()

macro(GIT_EXTERNAL_MESSAGE msg)
  if(COMMON_GIT_EXTERNAL_VERBOSE)
    message(STATUS "${NAME}: ${msg}")
  endif()
endmacro()

function(GIT_EXTERNAL DIR REPO tag)
  cmake_parse_arguments(GIT_EXTERNAL_LOCAL "VERBOSE;OPTIONAL" "" "RESET" ${ARGN})
  set(TAG ${tag})
  if(GIT_EXTERNAL_TAG AND "${tag}" MATCHES "^[0-9a-f]+$")
    set(TAG ${GIT_EXTERNAL_TAG})
  endif()

  # check if we had a previous external of the same name
  string(REGEX REPLACE "[:/]" "_" TARGET "${DIR}")
  get_property(OLD_TAG GLOBAL PROPERTY ${TARGET}_GITEXTERNAL_TAG)
  if(OLD_TAG)
    if(NOT OLD_TAG STREQUAL TAG)
      string(REPLACE "${CMAKE_SOURCE_DIR}/" "" PWD
        "${CMAKE_CURRENT_SOURCE_DIR}")
      git_external_message("${DIR}: already configured with ${OLD_TAG}, ignoring requested ${TAG} in ${PWD}")
      return()
    endif()
  else()
    set_property(GLOBAL PROPERTY ${TARGET}_GITEXTERNAL_TAG ${TAG})
  endif()

  if(NOT IS_ABSOLUTE "${DIR}")
    set(DIR "${CMAKE_SOURCE_DIR}/${DIR}")
  endif()
  get_filename_component(NAME "${DIR}" NAME)
  get_filename_component(GIT_EXTERNAL_DIR "${DIR}/.." ABSOLUTE)

  if(NOT EXISTS "${DIR}")
    # clone
    message(STATUS "git clone ${REPO} ${DIR} [${TAG}]")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" clone ${REPO} ${DIR}
      RESULT_VARIABLE nok ERROR_VARIABLE error
      WORKING_DIRECTORY "${GIT_EXTERNAL_DIR}")
    if(nok)
      if(GIT_EXTERNAL_LOCAL_OPTIONAL)
        message(STATUS "${DIR} clone failed: ${error}\n")
        return()
      else()
        message(FATAL_ERROR "${DIR} clone failed: ${error}\n")
      endif()
    endif()

    # checkout requested tag
    execute_process(COMMAND "${GIT_EXECUTABLE}" checkout -q "${TAG}"
      RESULT_VARIABLE nok ERROR_VARIABLE error WORKING_DIRECTORY "${DIR}")
    if(nok)
      message(FATAL_ERROR "git checkout ${TAG} in ${DIR} failed: ${error}\n")
    else()
      # init submodules of the subproject, excluding CMake/common if present
      execute_process(COMMAND "${GIT_EXECUTABLE}" submodule init
        WORKING_DIRECTORY "${DIR}" OUTPUT_QUIET)
      execute_process(COMMAND "${GIT_EXECUTABLE}" submodule deinit CMake/common
        WORKING_DIRECTORY "${DIR}" OUTPUT_QUIET ERROR_QUIET)
      execute_process(COMMAND "${GIT_EXECUTABLE}" submodule update --recursive
        WORKING_DIRECTORY "${DIR}")
    endif()
  endif()

  # set up "user" remote for github forks and make it default for 'git push'
  if(GITHUB_USER AND REPO MATCHES ".*github.com.*")
    string(REGEX REPLACE ".*(github.com)[\\/:]().*(\\/.*)" "git@\\1:\\2${GITHUB_USER}\\3"
      GIT_EXTERNAL_USER_REPO ${REPO})
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" remote add user ${GIT_EXTERNAL_USER_REPO}
      OUTPUT_QUIET ERROR_QUIET WORKING_DIRECTORY "${DIR}")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" config remote.pushdefault user
      OUTPUT_QUIET ERROR_QUIET WORKING_DIRECTORY "${DIR}")
  endif()

  if(COMMON_SOURCE_DIR)
    file(RELATIVE_PATH __dir ${COMMON_SOURCE_DIR} ${DIR})
  else()
    file(RELATIVE_PATH __dir ${CMAKE_SOURCE_DIR} ${DIR})
  endif()
  string(REGEX REPLACE "[:/\\.]" "-" __target "${__dir}")
  if(TARGET ${__target}-rebase)
    return()
  endif()

  set(__rebase_cmake "${CMAKE_CURRENT_BINARY_DIR}/${__target}-rebase.cmake")
  file(WRITE ${__rebase_cmake}
    "if(NOT IS_DIRECTORY \"${DIR}/.git\")\n"
    "  message(FATAL_ERROR \"Can't update git external ${__dir}: Not a git repository\")\n"
    "endif()\n"
    # check if we are already on the requested tag (nothing to do)
    "execute_process(COMMAND \"${GIT_EXECUTABLE}\" rev-parse --short HEAD\n"
    "  OUTPUT_VARIABLE currentref OUTPUT_STRIP_TRAILING_WHITESPACE\n"
    "  WORKING_DIRECTORY \"${DIR}\")\n"
    "if(currentref STREQUAL ${TAG}) # nothing to do\n"
    "  return()\n"
    "endif()\n"
    "\n"
    # reset generated files
    "foreach(GIT_EXTERNAL_RESET_FILE ${GIT_EXTERNAL_RESET})\n"
    "  execute_process(\n"
    "    COMMAND \"${GIT_EXECUTABLE}\" reset -q \"\${GIT_EXTERNAL_RESET_FILE}\"\n"
    "    ERROR_QUIET OUTPUT_QUIET\n"
    "    WORKING_DIRECTORY \"${DIR}\")\n"
    "  execute_process(\n"
    "    COMMAND \"${GIT_EXECUTABLE}\" checkout -q -- \"${GIT_EXTERNAL_RESET_FILE}\"\n"
    "    ERROR_QUIET OUTPUT_QUIET\n"
    "    WORKING_DIRECTORY \"${DIR}\")\n"
    "endforeach()\n"
    "\n"
    # fetch updates
    "execute_process(COMMAND \"${GIT_EXECUTABLE}\" fetch origin -q\n"
    "  RESULT_VARIABLE nok ERROR_VARIABLE error\n"
    "  WORKING_DIRECTORY \"${DIR}\")\n"
    "if(nok)\n"
    "  message(FATAL_ERROR \"Fetch for ${__dir} failed:\n   \${error}\")\n"
    "endif()\n"
    "\n"
  )
  if("${TAG}" MATCHES "^[0-9a-f]+$")
    # requested TAG is a SHA1, just switch to it
    file(APPEND ${__rebase_cmake}
      # checkout requested tag
      "execute_process(\n"
      "  COMMAND \"${GIT_EXECUTABLE}\" checkout -q \"${TAG}\"\n"
      "  RESULT_VARIABLE nok ERROR_VARIABLE error\n"
      "  WORKING_DIRECTORY \"${DIR}\")\n"
      "if(nok)\n"
      "  message(FATAL_ERROR \"git checkout ${TAG} in ${__dir} failed: \${error}\")\n"
    )
  else()
    # requested TAG is a branch
    file(APPEND ${__rebase_cmake}
      # switch to requested branch
      "execute_process(\n"
      "  COMMAND \"${GIT_EXECUTABLE}\" checkout -q \"${TAG}\"\n"
      "  OUTPUT_QUIET ERROR_QUIET WORKING_DIRECTORY \"${DIR}\")\n"
      # try to rebase it
      "execute_process(COMMAND \"${GIT_EXECUTABLE}\" rebase FETCH_HEAD\n"
      "  RESULT_VARIABLE nok ERROR_VARIABLE error OUTPUT_VARIABLE output\n"
      "  WORKING_DIRECTORY \"${DIR}\")\n"
      "if(nok)\n"
      "  execute_process(COMMAND \"${GIT_EXECUTABLE}\" rebase --abort\n"
      "    WORKING_DIRECTORY \"${DIR}\" ERROR_QUIET OUTPUT_QUIET)\n"
      "  message(FATAL_ERROR \"Rebase ${__dir} failed:\n\${output}\${error}\")\n"
    )
  endif()

  file(APPEND ${__rebase_cmake}
    # update submodules if checkout worked
    "else()\n"
    "  execute_process(\n"
    "    COMMAND \"${GIT_EXECUTABLE}\" submodule update --init --recursive\n"
    "    WORKING_DIRECTORY \"${DIR}\")\n"
    "endif()\n"
  )

  if(NOT GIT_EXTERNAL_SCRIPT_MODE)
    add_custom_target(${__target}-rebase
      COMMAND ${CMAKE_COMMAND} -P ${__rebase_cmake}
      COMMENT "Rebasing ${__dir} [${TAG}]")
    set_target_properties(${__target}-rebase PROPERTIES
      EXCLUDE_FROM_DEFAULT_BUILD ON FOLDER git)
    if(NOT TARGET rebase)
      add_custom_target(rebase)
      set_target_properties(rebase PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON)
    endif()
    add_dependencies(rebase ${__target}-rebase)
  endif()
endfunction()

