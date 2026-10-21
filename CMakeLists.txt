# gameengine2 — the top-level build. Targets:
#   ge_core      the deterministic pool and the run clock (engine/core, engine/loop;
#                headers, plus libm, solar and output compiled)
#   ge_<plugin>  one per plugins/<plugin>/ (cmake/Plugins.cmake): ge_racing (tape, records,
#                standings), ge_mountain (the generated ski mountain: mapgen, analysis)
#   ge_lab       the lab framework (engine/lab: the contract, the runner, the raster, the PNG)
#   labs         the lab executable: every labs/ directory's files (engine, plugins, games)
#   <x>_test     one executable per tests/<x>_test.cpp, registered with CTest
#   ge_fp        the FP flags of the DETERMINISM CONTRACT (AGENTS.md), linked by
#                ge_core and published as ge::fp; a target that drops them can
#                silently change the bits a simulation produces
#   ge_flags     warnings, -Werror, sanitizers: this tree only, never published
# ge_core, ge_fp and every plugin target are installed and published as ge::core,
# ge::fp and ge::<plugin> (cmake/Packaging.cmake, docs/publishing.md).
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
  engine/core/solar.cpp engine/core/output.cpp engine/core/num_text.cpp)
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

# The plugins: every plugins/<name>/CMakeLists.txt, each one ge_add_plugin(...) call. A new
# plugin is a new directory; nothing is listed here (docs/architecture.md § Plugins).
include(cmake/Plugins.cmake)
file(GLOB GE_PLUGIN_DIRS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/plugins/*/CMakeLists.txt)
foreach(plugin_cmake ${GE_PLUGIN_DIRS})
  get_filename_component(plugin_dir ${plugin_cmake} DIRECTORY)
  add_subdirectory(${plugin_dir})
endforeach()

# The games: games/<game>/CMakeLists.txt, once a game has code (Fall Line's lands with M5).
# A game is the demo, not part of the published engine, so only the top-level build makes it.
if(PROJECT_IS_TOP_LEVEL)
  file(GLOB GE_GAME_DIRS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/games/*/CMakeLists.txt)
  foreach(game_cmake ${GE_GAME_DIRS})
    get_filename_component(game_dir ${game_cmake} DIRECTORY)
    add_subdirectory(${game_dir})
  endforeach()

  # The lab framework, and the one `labs` executable every lab is linked into (docs/labs.md).
  # A lab registers itself from a static initializer, so labs are compiled into the executable,
  # never into a static library (whose unreferenced objects the linker would drop). Any file in
  # a `labs/` directory under engine/lab, plugins/<name> or games/<name> is part of it, and it
  # links every plugin, so a plugin's labs need no wiring. Labs are this tree's tooling, never
  # published.
  add_library(ge_lab STATIC engine/lab/lab.cpp engine/lab/report.cpp engine/lab/runner.cpp
    engine/lab/raster.cpp engine/lab/png.cpp)
  target_link_libraries(ge_lab PUBLIC ge_core)
  file(GLOB GE_LAB_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/lab/labs/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/*/labs/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/games/*/labs/*.cpp)
  get_property(GE_PLUGIN_TARGETS GLOBAL PROPERTY GE_PLUGIN_TARGETS)
  add_executable(labs engine/lab/main.cpp ${GE_LAB_SOURCES})
  target_link_libraries(labs PRIVATE ge_lab ge_core ${GE_PLUGIN_TARGETS})

  enable_testing()
  add_subdirectory(tests)
endif()

include(cmake/Packaging.cmake)
