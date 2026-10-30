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
    set(CMAKE_CXX_COMPILER_LAUNCHER "${GE_CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${GE_CCACHE_PROGRAM}")
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
