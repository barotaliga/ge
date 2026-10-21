# THE PLUGIN CONTRACT's build half (docs/architecture.md § Plugins). Every directory
# plugins/<name>/ with a CMakeLists.txt is a plugin; the root CMakeLists.txt adds them all, so a
# new plugin is a new directory and nothing is registered by hand. A plugin's CMakeLists.txt is
# one call:
#
#   ge_add_plugin(mountain ENGINE core PLUGINS racing)
#
# which builds every .cpp under the directory (labs/ excepted; app/ refused until it is split
# into its own target) into the target ge_<name>, puts
# plugins/ on its include path (so its headers are spelled "<name>/<file>.hpp"), and links the
# engine targets it names (ENGINE core -> ge_core) and the plugins it names (PLUGINS racing ->
# ge_racing). Those two lists are the plugin's whole dependency contract: tools/check-plugins.mjs
# (make agents-check) fails an #include outside them, a cycle, and a name that is not one
# lower-case word or that shadows an engine directory.
include_guard(GLOBAL)

function(ge_add_plugin name)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "ENGINE;PLUGINS")
  get_filename_component(dir_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
  if(NOT name STREQUAL dir_name)
    message(FATAL_ERROR "ge_add_plugin(${name}) is called from plugins/${dir_name}/. A plugin's "
      "name is its directory's: rename one of them (skill: plugins).")
  endif()
  if(NOT name MATCHES "^[a-z][a-z0-9]*$")
    message(FATAL_ERROR "plugin '${name}': a plugin name is one lower-case word, because it is "
      "the directory, the namespace ge::${name}, the target ge_${name} and the skill at once "
      "(skill: plugins).")
  endif()
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "ge_add_plugin(${name}): unknown arguments ${arg_UNPARSED_ARGUMENTS}; "
      "the only lists are ENGINE <module>... and PLUGINS <plugin>... (skill: plugins).")
  endif()

  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
  list(FILTER sources EXCLUDE REGEX "/labs/")
  set(app_sources ${sources})
  list(FILTER app_sources INCLUDE REGEX "/plugins/${name}/app/")
  if(app_sources)
    message(FATAL_ERROR "plugins/${name}/app/ has sources. A plugin's presentation half gets its "
      "own target (ge_${name}_app) so the simulation target never links a renderer: add that "
      "split to cmake/Plugins.cmake first (skill: plugins, § Add a plugin).")
  endif()
  set(target ge_${name})
  if(sources)
    add_library(${target} STATIC ${sources})
    set(scope PUBLIC)
  else()
    add_library(${target} INTERFACE)  # a header-only plugin
    set(scope INTERFACE)
  endif()
  # Installed, a plugin's headers sit beside the engine's (cmake/Packaging.cmake), so the include
  # root it inherits from ge_core serves it there.
  target_include_directories(${target} ${scope} "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/plugins>")
  foreach(module IN LISTS arg_ENGINE)
    target_link_libraries(${target} ${scope} ge_${module})
  endforeach()
  foreach(plugin IN LISTS arg_PLUGINS)
    target_link_libraries(${target} ${scope} ge_${plugin})
  endforeach()
  set_property(GLOBAL APPEND PROPERTY GE_PLUGIN_TARGETS ${target})
endfunction()
