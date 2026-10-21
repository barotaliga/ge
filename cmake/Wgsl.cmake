# WGSL EMBEDDING and the offline shader check (gpu skill, § Shaders).
#
#   ge_embed_wgsl(<target> <file.wgsl>...)
#
# turns each `<dir>/<name>.wgsl` (a path under engine/ or plugins/) into a generated header
# `<dir>/<name>.wgsl.hpp` on <target>'s include path, holding
#
#   namespace ge::wgsl { inline constexpr std::string_view <name> = R"..."; }
#
# with every `#include "<path>.wgsl"` line resolved first (searched beside the including file,
# then from engine/ and plugins/; each file once). No shader is read from disk at run time.
# The resolved text is also written beside the header as `<name>.wgsl`, and every one of them
# is compiled by Tint in the `shaders_check` target (make shaders-check), so a WGSL error fails
# with its file and line without a GPU. A shader's <name> is unique across the tree.
include_guard(GLOBAL)

set(GE_WGSL_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/WgslEmbed.cmake")

function(ge_embed_wgsl target)
  set(gen_root "${PROJECT_BINARY_DIR}/gen")
  file(GLOB_RECURSE all_wgsl CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/engine/*.wgsl" "${PROJECT_SOURCE_DIR}/plugins/*.wgsl")
  foreach(src IN LISTS ARGN)
    get_filename_component(abs "${src}" ABSOLUTE)
    file(RELATIVE_PATH rel "${PROJECT_SOURCE_DIR}" "${abs}")
    string(REGEX REPLACE "^(engine|plugins)/" "" rel "${rel}")
    get_filename_component(name "${abs}" NAME_WE)
    set(header "${gen_root}/${rel}.hpp")
    get_filename_component(dir "${gen_root}/${rel}" DIRECTORY)
    set(resolved "${dir}/${name}.wgsl")
    add_custom_command(OUTPUT "${header}" "${resolved}"
      COMMAND ${CMAKE_COMMAND} -DIN=${abs} -DHEADER=${header} -DRESOLVED=${resolved}
              -DNAME=${name} -DROOTS=${PROJECT_SOURCE_DIR}/engine|${PROJECT_SOURCE_DIR}/plugins
              -P "${GE_WGSL_SCRIPT}"
      DEPENDS "${abs}" ${all_wgsl} "${GE_WGSL_SCRIPT}"
      COMMENT "wgsl: ${rel}"
      VERBATIM)
    target_sources(${target} PRIVATE "${header}")
    set_property(GLOBAL APPEND PROPERTY GE_WGSL_RESOLVED "${resolved}")
  endforeach()
  target_include_directories(${target} PUBLIC "$<BUILD_INTERFACE:${gen_root}>")
endfunction()

# The shaders_check target, once every ge_embed_wgsl call has run (the root CMakeLists.txt calls
# this last): every entry point of every shader compiled to SPIR-V by Tint
# (cmake/WgslCheck.cmake). It needs Tint (GE_TINT, from cmake/Dawn.cmake).
function(ge_add_shaders_check)
  get_property(resolved GLOBAL PROPERTY GE_WGSL_RESOLVED)
  set(stamps)
  foreach(wgsl IN LISTS resolved)
    set(stamp "${wgsl}.checked")
    add_custom_command(OUTPUT "${stamp}"
      COMMAND ${CMAKE_COMMAND} -DTINT=${GE_TINT} -DWGSL=${wgsl} -DSTAMP=${stamp}
              -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/WgslCheck.cmake"
      DEPENDS "${wgsl}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/WgslCheck.cmake"
      COMMENT "tint: ${wgsl}"
      VERBATIM)
    list(APPEND stamps "${stamp}")
  endforeach()
  add_custom_target(shaders_check DEPENDS ${stamps})
endfunction()
