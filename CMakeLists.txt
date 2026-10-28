# gameengine2 — the top-level build. Targets:
#   ge_<module>  one per engine/<module>/module.cmake (cmake/Modules.cmake), each file saying what
#                its module is: ge_core (engine/core and engine/loop), ge_audio, ge_assets, ge_net,
#                ge_console, ge_services, ge_snow, ge_input, ge_wind, ge_water, ge_move, ge_camera, ge_lab,
#                ge_ai, ge_prof and ge_prof_alloc, ge_play (with engine/capture), ge_replay; and, when their layer is built,
#                ge_platform (GE_PLATFORM), ge_gpu and ge_render (GE_GPU)
#   ge_cli, ge   the ge command line (engine/cli, cmake/Cli.cmake)
#   ge_<plugin>  one per plugins/<plugin>/ (cmake/Plugins.cmake): ge_racing, ge_mountain, ...
#   fall_line_sim  Fall Line's simulation (games/fall_line: create_game, step, the bot)
#   labs         the lab executable: every labs/ directory's files (engine, plugins, games)
#   assets       the built assets listed, checked and stamped (engine/assets/tool.hpp)
#   <x>_test     one executable per tests/<x>_test.cpp, registered with CTest
#   ge_fp        the FP flags of the DETERMINISM CONTRACT (AGENTS.md), linked by
#                ge_core and published as ge::fp; a target that drops them can
#                silently change the bits a simulation produces
#   ge_isa       the instruction-set baseline (x86-64: AVX2), linked by ge_core and
#                published as ge::isa
#   ge_flags     warnings, -Werror, sanitizers: this tree only, never published
# Every module registered with an EXPORT name, and every plugin target, is installed and published
# as ge::<name>, with ge::fp and ge::isa, and the ge executable is installed to bin/
# (cmake/Packaging.cmake, docs/publishing.md).
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

# The instruction-set baseline. On x86-64 the engine requires AVX2 (x86-64-v3: AVX2, FMA, BMI2,
# every desktop x86 CPU since 2013), so the compiler vectorises with 256-bit registers; ARM64's
# baseline already has NEON. It changes no bits: ge_fp keeps contraction off, so the FMA
# instructions it allows are never formed from a*b+c. It travels with ge_core, so a game is
# built for the same CPUs as the engine it links.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
  set(GE_ISA_X86 ON)
endif()
add_library(ge_isa INTERFACE)
if(GE_ISA_X86)
  target_compile_options(ge_isa INTERFACE
    "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-march=x86-64-v3>"
    "$<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>")
endif()

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
  # C4996 calls standard C (fopen, getenv, sprintf) unsafe and, under /WX, fails code that GCC
  # and Clang build as portable. The tree uses the standard calls on every OS.
  target_compile_definitions(ge_flags INTERFACE _CRT_SECURE_NO_WARNINGS)
  if(GE_WERROR)
    target_compile_options(ge_flags INTERFACE /WX)
  endif()
endif()

# The engine's modules: every engine/<module>/module.cmake, each defining its target and
# registering it with one ge_module(...) call (cmake/Modules.cmake). A new module is a new
# directory; nothing is listed here, in the package, the tests or the checks.
include(cmake/Modules.cmake)
# What the modules build on: the GPU's and the platform's dependencies, the shader embedder.
include(cmake/Dawn.cmake)
include(cmake/SDL.cmake)
include(cmake/Wgsl.cmake)
file(GLOB GE_MODULE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/*/module.cmake)
foreach(module_cmake ${GE_MODULE_FILES})
  include(${module_cmake})
endforeach()
ge_linked_modules(GE_MODULE_TARGETS)

# The plugins: every plugins/<name>/CMakeLists.txt, each one ge_add_plugin(...) call. A new
# plugin is a new directory; nothing is listed here (docs/architecture.md § Plugins).
include(cmake/Plugins.cmake)
file(GLOB GE_PLUGIN_DIRS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/plugins/*/CMakeLists.txt)
foreach(plugin_cmake ${GE_PLUGIN_DIRS})
  get_filename_component(plugin_dir ${plugin_cmake} DIRECTORY)
  add_subdirectory(${plugin_dir})
endforeach()

# The games: games/<game>/CMakeLists.txt, each adding its targets to GE_GAME_TARGETS, which the
# labs and the tests link.
# A game is the demo, not part of the published engine, so only the top-level build makes it.
if(PROJECT_IS_TOP_LEVEL)
  file(GLOB GE_GAME_DIRS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/games/*/CMakeLists.txt)
  foreach(game_cmake ${GE_GAME_DIRS})
    get_filename_component(game_dir ${game_cmake} DIRECTORY)
    add_subdirectory(${game_dir})
  endforeach()

  # The one `labs` executable every lab is linked into (docs/labs.md).
  # A lab registers itself from a static initializer, so labs are compiled into the executable,
  # never into a static library (whose unreferenced objects the linker would drop). Any file in
  # a `labs/` directory under engine/lab, plugins/<name> or games/<name> is part of it, and it
  # links every plugin, so a plugin's labs need no wiring. This tree's labs are never published;
  # a game builds its own `labs` executable over ge::lab.
  file(GLOB GE_LAB_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/lab/labs/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/*/labs/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/games/*/labs/*.cpp)
  # GPU and window labs (gpu_*_lab.cpp, platform_*_lab.cpp, in any labs/ directory) build only
  # with the layer they measure; a game's GPU labs link its app library (<game>_app).
  set(GE_LAB_EXTRA "")
  if(GE_GPU)
    list(APPEND GE_LAB_EXTRA ge_gpu ge_render)
    get_property(GE_GAME_APPS GLOBAL PROPERTY GE_GAME_APPS)
    list(APPEND GE_LAB_EXTRA ${GE_GAME_APPS})
  else()
    list(FILTER GE_LAB_SOURCES EXCLUDE REGEX "/labs/gpu_[^/]*$")
  endif()
  if(GE_PLATFORM AND GE_GPU)
    list(APPEND GE_LAB_EXTRA ge_platform)
  else()
    list(FILTER GE_LAB_SOURCES EXCLUDE REGEX "/engine/lab/labs/platform_[^/]*$")
  endif()
  # The ge command line's lab (cli_*_lab.cpp) builds only with the command line.
  if(GE_CLI)
    list(APPEND GE_LAB_EXTRA ge_cli)
  else()
    list(FILTER GE_LAB_SOURCES EXCLUDE REGEX "/labs/cli_[^/]*$")
  endif()
  get_property(GE_PLUGIN_TARGETS GLOBAL PROPERTY GE_PLUGIN_TARGETS)
  get_property(GE_GAME_TARGETS GLOBAL PROPERTY GE_GAME_TARGETS)
  add_executable(labs engine/lab/main.cpp ${GE_LAB_SOURCES})
  target_link_libraries(labs PRIVATE ${GE_MODULE_TARGETS} ${GE_PLUGIN_TARGETS} ${GE_GAME_TARGETS}
    ${GE_LAB_EXTRA})
  if(GE_GPU)
    # A GPU lab's own shader (the eye lab's hall, drawn over its frame).
    ge_embed_wgsl(labs engine/lab/labs/gpu_eye_hall.wgsl)
  endif()

  # The `assets` executable (engine/assets/tool.hpp): the built assets listed, checked and stamped,
  # and the game data their builders are fed. A data set registers itself from a static
  # initializer in an `assets/<name>/data.cpp` beside its builder, so, as with the labs, those
  # files are compiled into the executable, which links every plugin and game.
  file(GLOB_RECURSE GE_ASSET_DATA_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/*/assets/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/games/*/assets/*.cpp)
  add_executable(assets engine/assets/main.cpp ${GE_ASSET_DATA_SOURCES})
  target_link_libraries(assets PRIVATE ge_assets ge_core ${GE_PLUGIN_TARGETS} ${GE_GAME_TARGETS})

  enable_testing()
  add_subdirectory(tests)
endif()

# Every embedded shader through Tint: `make shaders-check` (after every ge_embed_wgsl call).
if(GE_GPU AND GE_TINT)
  ge_add_shaders_check()
endif()

include(cmake/Packaging.cmake)
