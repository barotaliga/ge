# THE LINKER: on Linux every executable and shared library links with mold or LLD when one is
# installed, in place of GNU ld. The tree links some 500 executables (a test each, the labs, the
# tools), so after the compiler cache has taken the compiles, linking is most of a rebuild: LLD
# cut a debug rebuild from a full cache from 105 s to 74 s (docs/architecture.md § Build).
# Included by the root CMakeLists.txt; nothing of it reaches an installed package.
#
# -DGE_FAST_LINKER=OFF links with the compiler's default. macOS's and MSVC's own linkers are
# already fast and are left alone. The linker writes no floating-point bits: the goldens and
# the digests are the same whichever links them.
include_guard(GLOBAL)
include(CheckLinkerFlag)

option(GE_FAST_LINKER "Link with mold or LLD on Linux when one is installed" ON)

if(GE_FAST_LINKER AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  foreach(ge_linker mold lld)
    check_linker_flag(CXX "-fuse-ld=${ge_linker}" GE_HAS_LINKER_${ge_linker})
    if(GE_HAS_LINKER_${ge_linker})
      add_link_options("-fuse-ld=${ge_linker}")
      message(STATUS "ge: linking with ${ge_linker}")
      break()
    endif()
  endforeach()
endif()
