# THE COMPILER CACHE: every C and C++ compile goes through ccache (or sccache) when one is on PATH,
# so a rebuild of what was built before, in this tree or a fresh checkout of it, takes the object
# files from the cache instead of compiling them again. Included by the root CMakeLists.txt.
#
# A launcher already chosen wins: -DCMAKE_CXX_COMPILER_LAUNCHER=..., or the environment variable
# of the same name (CI sets it). -DGE_CCACHE=OFF compiles without a cache. Install one with
# `tools/setup-macos.sh` (macOS), `tools/setup-ubuntu.sh` (Linux) or `choco install ccache`
# (Windows); `ccache --show-stats` says how many compiles it saved. docs/architecture.md § The
# build has the measurements.
#
# MSVC writes /Zi debug information to a shared .pdb, which no cache can store, so with a cache
# the debug configurations embed it in each object instead (/Z7). Release builds have none.
include_guard(GLOBAL)

option(GE_CCACHE "Compile through ccache (or sccache) when one is on PATH" ON)

if(GE_CCACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
  find_program(GE_CCACHE_PROGRAM NAMES ccache sccache)
  if(GE_CCACHE_PROGRAM)
    set(ge_launcher "${GE_CCACHE_PROGRAM}")
    # ccache 4.8+ takes settings before the compiler. Two make the keys the same on every machine
    # that builds this tree: base_dir hashes paths under the checkout as relative ones (another
    # checkout path, a worktree), and compiler_check=content hashes the compiler by its bytes,
    # not by its file's time (the same compiler installed on another day in another container).
    # Without them a cache carried to a new container (tools/ccache-share.sh) can miss on every
    # compile.
    get_filename_component(ge_launcher_name "${GE_CCACHE_PROGRAM}" NAME_WE)
    if(ge_launcher_name STREQUAL "ccache")
      execute_process(COMMAND "${GE_CCACHE_PROGRAM}" --version OUTPUT_VARIABLE ge_ccache_out
                      ERROR_QUIET)
      if(ge_ccache_out MATCHES "version ([0-9]+\\.[0-9]+)" AND CMAKE_MATCH_1 VERSION_GREATER_EQUAL 4.8)
        list(APPEND ge_launcher "base_dir=${PROJECT_SOURCE_DIR}" "compiler_check=content")
      endif()
    endif()
    set(CMAKE_CXX_COMPILER_LAUNCHER "${ge_launcher}")
    set(CMAKE_C_COMPILER_LAUNCHER "${ge_launcher}")
  endif()
endif()
if(CMAKE_CXX_COMPILER_LAUNCHER AND NOT CMAKE_C_COMPILER_LAUNCHER)
  set(CMAKE_C_COMPILER_LAUNCHER "${CMAKE_CXX_COMPILER_LAUNCHER}")
endif()

if(CMAKE_CXX_COMPILER_LAUNCHER)
  message(STATUS "ge: compiling through ${CMAKE_CXX_COMPILER_LAUNCHER}")
  if(MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
  endif()
else()
  message(STATUS "ge: no compiler cache (install ccache to rebuild in seconds what was built before)")
endif()
