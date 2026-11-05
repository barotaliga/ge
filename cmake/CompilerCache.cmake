# THE COMPILER CACHE: every C and C++ compile goes through ccache (or sccache) when one is on PATH,
# so a rebuild of what was built before, in this tree or a fresh checkout of it, takes the object
# files from the cache instead of compiling them again. Included by the root CMakeLists.txt.
#
# A launcher already chosen wins: -DCMAKE_CXX_COMPILER_LAUNCHER=..., or the environment variable
# of the same name (CI sets it). -DGE_CCACHE=OFF compiles without a cache. Install one with
# `tools/setup-macos.sh` (macOS), `tools/setup-ubuntu.sh` (Linux) or `tools/setup-windows.ps1`
# (Windows); `ccache --show-stats` says how many compiles it saved. docs/architecture.md § The
# build has the measurements.
#
# MSVC writes /Zi debug information to a shared .pdb, which no cache can store, so with a cache
# the debug configurations embed it in each object instead (/Z7). Release builds have none.
#
# A GIT WORKTREE builds warm with no setup of its own. Its checkout is new, but its compiles are the
# main checkout's: base_dir makes their paths relative, so they hash the same in every checkout,
# and the build folder is mapped out of the debug information (-fdebug-prefix-map=<build>=.), so a
# debug build's object no longer names the folder it was built in and ccache need not hash it
# (hash_dir=false): measured, a -g compile in a second checkout hits only with both. The worktree's
# downloads are the main checkout's too (GE_DEPS_DIR defaults to its .cache/deps: Dawn, SDL and
# the codecs are not fetched again), and while ccache's size is its own default (5 GiB, too small
# for a few checkouts' debug and release objects: it evicts what the next worktree needs) the
# worktree's compiles raise it to GE_CCACHE_WORKTREE_SIZE (10G).
include_guard(GLOBAL)

# The main checkout when this tree is a git worktree of it (empty otherwise).
set(ge_main_checkout "")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse
                          --path-format=absolute --git-common-dir
                  OUTPUT_VARIABLE ge_git_common OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
                  RESULT_VARIABLE ge_git_status)
  if(ge_git_status EQUAL 0 AND ge_git_common MATCHES "/\\.git$"
     AND NOT ge_git_common STREQUAL "${PROJECT_SOURCE_DIR}/.git")
    get_filename_component(ge_main_checkout "${ge_git_common}" DIRECTORY)
  endif()
endif()
if(ge_main_checkout)
  message(STATUS "ge: a worktree of ${ge_main_checkout}: its downloads and compiler cache shared")
  # The tree's own default yields too (a build folder configured before this ran), and every path
  # found under it is looked for again under the shared one.
  set(ge_own_deps "${PROJECT_SOURCE_DIR}/.cache/deps")
  if(NOT GE_DEPS_DIR OR GE_DEPS_DIR STREQUAL ge_own_deps)
    set(GE_DEPS_DIR "${ge_main_checkout}/.cache/deps" CACHE PATH
        "Where prebuilt dependencies are downloaded and unpacked" FORCE)
  endif()
  if(NOT GE_DEPS_DIR STREQUAL ge_own_deps)
    get_cmake_property(ge_cached CACHE_VARIABLES)
    foreach(ge_var IN LISTS ge_cached)
      get_property(ge_value CACHE "${ge_var}" PROPERTY VALUE)
      string(FIND "${ge_value}" "${ge_own_deps}/" ge_at)
      if(ge_at EQUAL 0)
        unset(${ge_var} CACHE)
      endif()
    endforeach()
  endif()
endif()
set(GE_CCACHE_WORKTREE_SIZE "10G" CACHE STRING
    "ccache's size in a worktree while ccache's own is its default (empty: leave it)")

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
        if(NOT MSVC)
          list(APPEND ge_launcher "hash_dir=false")
          set(ge_ccache_debug_map ON)
        endif()
        if(ge_main_checkout AND GE_CCACHE_WORKTREE_SIZE)
          execute_process(COMMAND "${GE_CCACHE_PROGRAM}" --show-config OUTPUT_VARIABLE ge_ccache_cfg
                          ERROR_QUIET)
          if(ge_ccache_cfg MATCHES "\\(default\\) max_size")
            list(APPEND ge_launcher "max_size=${GE_CCACHE_WORKTREE_SIZE}")
          endif()
        endif()
      endif()
    endif()
    set(CMAKE_CXX_COMPILER_LAUNCHER "${ge_launcher}")
    set(CMAKE_C_COMPILER_LAUNCHER "${ge_launcher}")
  endif()
endif()
if(CMAKE_CXX_COMPILER_LAUNCHER AND NOT CMAKE_C_COMPILER_LAUNCHER)
  set(CMAKE_C_COMPILER_LAUNCHER "${CMAKE_CXX_COMPILER_LAUNCHER}")
endif()

if(ge_ccache_debug_map)
  # The build folder out of the debug information (above): every checkout's object the same.
  add_compile_options("-fdebug-prefix-map=${CMAKE_BINARY_DIR}=.")
endif()
if(CMAKE_CXX_COMPILER_LAUNCHER)
  message(STATUS "ge: compiling through ${CMAKE_CXX_COMPILER_LAUNCHER}")
  if(MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
  endif()
else()
  message(STATUS "ge: no compiler cache (install ccache to rebuild in seconds what was built before)")
endif()
