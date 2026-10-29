# THE ENGINE'S MODULES, each registered by its own directory. Every engine/<module>/module.cmake
# is included by the root CMakeLists.txt (in name order, before the plugins), defines its target
# with plain CMake and ends with one call:
#
#   ge_module(ge_move EXPORT move SIMULATION)
#
# Everything that used to list the engine's targets by hand reads that registration instead: the
# tests and the labs link every exported module, the package exports each under ge::<name>
# (cmake/Packaging.cmake) and installs the headers of exported modules only,
# tools/check-plugins.mjs knows which directory builds into which target, and
# tools/check-libm.sh which directories are simulation code. So a new module is a new directory
# with a module.cmake (and its AGENTS.md row), and no shared file is edited: parallel pull
# requests adding modules do not conflict.
#
#   ge_module(<target> [EXPORT <name>] [DIRS <dir>...] [SIMULATION] [OPT_IN])
#
#   EXPORT      published as ge::<name>, and linked by every test and the labs; a module without
#               it (gpu, render, platform, cli) is neither, and its headers are not installed
#   DIRS        the engine/ directories built into it (the module.cmake's own by default; core
#               also holds loop/)
#   SIMULATION  its code is simulation code: no <cmath> transcendental (tools/check-libm.sh)
#   OPT_IN      exported, but linked only by what asks for it (ge_prof_alloc replaces operator
#               new for the executable that links it)
#
# The call is made whether or not the target is built (a module behind GE_GPU registers before its
# `if`), so the package still knows which headers to leave out; a registered target that does not
# exist in this build is skipped by everything that reads the list. tools/check-plugins.mjs fails
# an engine/ directory that no module.cmake claims.
include_guard(GLOBAL)

function(ge_module target)
  cmake_parse_arguments(PARSE_ARGV 1 arg "SIMULATION;OPT_IN" "EXPORT" "DIRS")
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "ge_module(${target}): unknown arguments ${arg_UNPARSED_ARGUMENTS}; "
      "the call is ge_module(<target> [EXPORT <name>] [DIRS <dir>...] [SIMULATION] [OPT_IN]); "
      "next: fix the call in ${CMAKE_CURRENT_LIST_FILE} (see cmake/Modules.cmake).")
  endif()
  if(NOT arg_DIRS)
    get_filename_component(dir "${CMAKE_CURRENT_LIST_DIR}" NAME)
    set(arg_DIRS ${dir})
  endif()
  set_property(GLOBAL APPEND PROPERTY GE_MODULE_DIRS_${target} ${arg_DIRS})
  if(arg_EXPORT)
    set_property(GLOBAL APPEND PROPERTY GE_MODULE_EXPORTS ${target} ${arg_EXPORT})
    if(NOT arg_OPT_IN)
      set_property(GLOBAL APPEND PROPERTY GE_MODULE_LINKED ${target})
    endif()
  else()
    set_property(GLOBAL APPEND PROPERTY GE_MODULE_UNEXPORTED_DIRS ${arg_DIRS})
  endif()
endfunction()

# The modules every test and the labs link: each exported, not opt-in, and built here.
function(ge_linked_modules out)
  get_property(linked GLOBAL PROPERTY GE_MODULE_LINKED)
  set(built "")
  foreach(t IN LISTS linked)
    if(TARGET ${t})
      list(APPEND built ${t})
    endif()
  endforeach()
  set(${out} ${built} PARENT_SCOPE)
endfunction()
