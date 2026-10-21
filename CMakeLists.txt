# gameengine2 — the top-level build. Targets:
#   ge_core      the deterministic pool and the run clock (engine/core, engine/loop;
#                headers, plus libm, solar and output compiled)
#   ge_racing    the racing primitives (engine/racing: tape, records, standings)
#   fall_line_sim  game5's engine/ ported (games/fall_line/sim: mapgen, analysis)
#   <x>_test     one executable per tests/<x>_test.cpp, registered with CTest
#   ge_fp        the FP flags of the DETERMINISM CONTRACT (AGENTS.md), linked by
#                ge_core and published as ge::fp; a target that drops them can
#                silently change the bits a simulation produces
#   ge_flags     warnings, -Werror, sanitizers: this tree only, never published
# ge_core, ge_racing and ge_fp are installed and published as ge::core,
# ge::racing and ge::fp (cmake/Packaging.cmake, docs/publishing.md).
cmake_minimum_required(VERSION 3.25)
project(gameengine2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GE_WERROR "Treat warnings as errors" ON)
option(GE_SANITIZE "Build with address + undefined sanitizers" OFF)

# Two flag sets. ge_fp is the floating-point half of the DETERMINISM CONTRACT
# and travels with ge_core into an installed package, so a game built against
# a release computes the same bits as the engine's tests. Its flags are
# generator expressions so a package picks them by the CONSUMER's compiler.
# ge_flags is this tree's own hygiene (warnings, -Werror, sanitizers) and
# never leaves it.
add_library(ge_fp INTERFACE)
target_compile_options(ge_fp INTERFACE
  # Determinism: no fused multiply-add contraction, no fast-math reassociation.
  "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-ffp-contract=off;-fno-fast-math>"
  "$<$<CXX_COMPILER_ID:GNU>:-fexcess-precision=standard>"
  "$<$<CXX_COMPILER_ID:MSVC>:/fp:strict>")

add_library(ge_flags INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ge_flags INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
  if(GE_WERROR)
    target_compile_options(ge_flags INTERFACE -Werror)
  endif()
  if(GE_SANITIZE)
    target_compile_options(ge_flags INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ge_flags INTERFACE -fsanitize=address,undefined)
  endif()
elseif(MSVC)
  target_compile_options(ge_flags INTERFACE /W4 /permissive-)
  if(GE_WERROR)
    target_compile_options(ge_flags INTERFACE /WX)
  endif()
endif()

# The deterministic pool. Mostly headers; libm, solar and output are compiled.
set(GE_LIBM_VENDORED
  engine/core/libm/fdlibm_trig.cpp
  engine/core/libm/fdlibm_arc.cpp
  engine/core/libm/fdlibm_exp_log.cpp)
add_library(ge_core STATIC ${GE_LIBM_VENDORED} engine/core/libm/pow.cpp
  engine/core/solar.cpp engine/core/output.cpp engine/core/js_text.cpp)
target_include_directories(ge_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/engine>)
target_compile_features(ge_core PUBLIC cxx_std_20)
target_link_libraries(ge_core PUBLIC ge_fp)
# Our warnings only when this tree is the top-level build: a game that pulls it in
# with FetchContent, or links the installed package, gets ge_fp alone.
if(PROJECT_IS_TOP_LEVEL)
  target_link_libraries(ge_core PUBLIC $<BUILD_INTERFACE:ge_flags>)
endif()
# fdlibm is kept line for line with V8's copy; its deliberate int/double word conversions
# are not rewritten to quiet warnings. The FP flags above still apply to it.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_source_files_properties(${GE_LIBM_VENDORED} PROPERTIES COMPILE_OPTIONS "-Wno-conversion")
elseif(MSVC)
  set_source_files_properties(${GE_LIBM_VENDORED} PROPERTIES COMPILE_OPTIONS "/wd4244;/wd4146;/wd4701;/wd4723")
endif()

# The racing primitives. Headers, plus the tape's codec compiled.
add_library(ge_racing STATIC engine/racing/tape.cpp)
target_link_libraries(ge_racing PUBLIC ge_core)

# The demo game's simulation (games/fall_line/sim). It is the demo, not part of the
# published engine, so only the top-level build makes it.
if(PROJECT_IS_TOP_LEVEL)
  file(GLOB_RECURSE FALL_LINE_SIM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/games/fall_line/sim/*.cpp)
  add_library(fall_line_sim STATIC ${FALL_LINE_SIM_SOURCES})
  target_include_directories(fall_line_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/games/fall_line/sim)
  target_link_libraries(fall_line_sim PUBLIC ge_core)

  enable_testing()
  add_subdirectory(tests)
endif()

include(cmake/Packaging.cmake)
