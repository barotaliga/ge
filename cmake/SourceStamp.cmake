# ge_source_stamp(<target> <name> CONSTANT <kName> [SHADERS] DIRS <dir>...): a constant that changes whenever a source file
# under the DIRS changes, for a content cache's key (engine/save/cache.hpp): content built by that
# code is keyed by its stamp, so changed code never reads a stale entry. Writes the header
# <name>.hpp (`inline constexpr const char <kName>[]`, the SHA-256 of every *.cpp, *.hpp and *.h
# file's bytes and path under the DIRS, in path order) into the build tree, rebuilt when any of
# them changes; the target includes it as "stamps/<name>.hpp". DIRS are relative to the project.
# SHADERS adds every *.wgsl file under the DIRS (for a cache of pictures the GPU drew).
function(ge_source_stamp target name)
  cmake_parse_arguments(PARSE_ARGV 2 arg "SHADERS" "CONSTANT" "DIRS")
  set(globs "")
  foreach(dir IN LISTS arg_DIRS)
    list(APPEND globs "${PROJECT_SOURCE_DIR}/${dir}/*.cpp" "${PROJECT_SOURCE_DIR}/${dir}/*.hpp"
         "${PROJECT_SOURCE_DIR}/${dir}/*.h")
    if(arg_SHADERS)
      list(APPEND globs "${PROJECT_SOURCE_DIR}/${dir}/*.wgsl")
    endif()
  endforeach()
  file(GLOB_RECURSE files CONFIGURE_DEPENDS ${globs})
  list(SORT files)
  set(dir "${CMAKE_CURRENT_BINARY_DIR}/stamps_${target}")
  set(list_file "${dir}/${name}.files")
  set(out "${dir}/stamps/${name}.hpp")
  string(REPLACE ";" "\n" lines "${files}")
  file(CONFIGURE OUTPUT "${list_file}" CONTENT "${lines}\n")
  add_custom_command(
    OUTPUT "${out}"
    COMMAND ${CMAKE_COMMAND} -DLIST=${list_file} -DOUT=${out} -DNAME=${arg_CONSTANT}
            -DROOT=${PROJECT_SOURCE_DIR} -P ${PROJECT_SOURCE_DIR}/cmake/SourceStampScript.cmake
    DEPENDS ${files} "${list_file}" "${PROJECT_SOURCE_DIR}/cmake/SourceStampScript.cmake"
    COMMENT "Stamping ${name}"
    VERBATIM)
  target_sources(${target} PRIVATE "${out}")
  target_include_directories(${target} PRIVATE "${dir}")
endfunction()
