
# Create an empty target, exclude it from the default build, and add it to
# the given IDE folder

macro(common_target NAME FOLDER)
  if(NOT TARGET ${NAME})
    add_custom_target(${NAME})
    set_target_properties(${NAME} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON
      FOLDER ${NAME}/${FOLDER})
  endif()
endmacro()
