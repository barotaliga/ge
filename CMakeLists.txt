# gameengine2 — the top-level build. Targets:
#   ge_core      the deterministic pool and the run clock (engine/core, engine/loop;
#                headers, plus libm, solar and output compiled)
#   ge_<plugin>  one per plugins/<plugin>/ (cmake/Plugins.cmake): ge_racing (tape, records,
#                standings), ge_mountain (the generated ski mountain: mapgen, analysis)
#   ge_audio     the synth: voices, layers, buses, echo, limiter, the meter, WAV bytes
#                (engine/audio; deterministic, no platform)
#   ge_assets    authored models: the glTF loader (cgltf), the Model, posing, CPU skinning
#                (engine/assets; cmake/Cgltf.cmake)
#   ge_net       the network: links, the simulated network, captures, connections, nodes
#                (engine/net; docs/networking.md)
#   ge_lab       the lab framework (engine/lab: the contract, the runner, the raster, the PNG,
#                the series files and the charts)
#   ge_play      the play contract (engine/play: GameSpec and Run, the session, the input log,
#                the bench, the stills `ge shot` draws)
#   ge_prof      the profiler (engine/prof: timing zones, allocation counts, the resident set);
#                ge_prof_alloc the counting operator new an executable links to count allocations
#   ge_cli, ge   the ge command line (engine/cli: ge new, ge skills sync, ge doctor, ge lessons;
#                cmake/Cli.cmake)
#   fall_line_sim  Fall Line's simulation (games/fall_line: create_game, step, the bot)
#   labs         the lab executable: every labs/ directory's files (engine, plugins, games)
#   ge_platform  the window, input and the frame clock over SDL3 (engine/platform; GE_PLATFORM,
#                cmake/SDL.cmake)
#   ge_gpu       the native GPU device over Dawn (engine/gpu; GE_GPU, cmake/Dawn.cmake), with its
#                shaders embedded (cmake/Wgsl.cmake) and checked by `shaders_check`
#   <x>_test     one executable per tests/<x>_test.cpp, registered with CTest
#   ge_fp        the FP flags of the DETERMINISM CONTRACT (AGENTS.md), linked by
#                ge_core and published as ge::fp; a target that drops them can
#                silently change the bits a simulation produces
#   ge_isa       the instruction-set baseline (x86-64: AVX2), linked by ge_core and
#                published as ge::isa
#   ge_flags     warnings, -Werror, sanitizers: this tree only, never published
# ge_core, ge_fp, ge_isa, ge_assets, ge_net, ge_lab, ge_play, ge_prof, ge_prof_alloc and every plugin target are
# installed and published as ge::core, ge::fp, ge::isa, ge::assets, ge::net, ge::lab, ge::play, ge::prof,
# ge::prof_alloc and ge::<plugin>, and the ge executable is installed to bin/
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
  if(GE_WERROR)
    target_compile_options(ge_flags INTERFACE /WX)
  endif()
endif()

# The deterministic pool. Mostly headers; libm, solar and output are compiled.
# Threads: core/parallel.hpp runs a bake's independent rows side by side.
find_package(Threads REQUIRED)
set(GE_LIBM_VENDORED
  engine/core/libm/fdlibm_trig.cpp
  engine/core/libm/fdlibm_arc.cpp
  engine/core/libm/fdlibm_exp_log.cpp)
add_library(ge_core STATIC ${GE_LIBM_VENDORED} engine/core/libm/pow.cpp
  engine/core/solar.cpp engine/core/output.cpp engine/core/num_text.cpp)
target_include_directories(ge_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/engine>)
target_compile_features(ge_core PUBLIC cxx_std_20)
target_link_libraries(ge_core PUBLIC ge_fp ge_isa Threads::Threads)
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

# The synth (M9): plain C++ over the core's libm and stream, so it builds, renders and is
# metered with no platform; engine/platform's audio device plays it.
file(GLOB GE_AUDIO_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/audio/*.cpp)
add_library(ge_audio STATIC ${GE_AUDIO_SOURCES})
target_link_libraries(ge_audio PUBLIC ge_core)

# Authored models (M8e): the glTF loader (cgltf, cmake/Cgltf.cmake, compiled into
# engine/assets/gltf.cpp alone), the Model it fills, posing and CPU skinning. No GPU and no
# platform; render/skinning.hpp skins on the GPU.
include(cmake/Cgltf.cmake)
file(GLOB GE_ASSETS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/assets/*.cpp)
add_library(ge_assets STATIC ${GE_ASSETS_SOURCES})
target_link_libraries(ge_assets PUBLIC ge_core)
target_include_directories(ge_assets SYSTEM PRIVATE ${GE_CGLTF_INCLUDE})

# The platform and the GPU (M6). Each is optional (GE_PLATFORM, GE_GPU) and nothing else
# depends on them: the simulation, the plugins and their tests build without either.
include(cmake/Dawn.cmake)
include(cmake/SDL.cmake)
include(cmake/Wgsl.cmake)
if(GE_PLATFORM)
  add_library(ge_platform STATIC engine/platform/window.cpp engine/platform/audio.cpp)
  target_link_libraries(ge_platform PUBLIC ge_core PRIVATE SDL3::SDL3-static)
endif()
if(GE_GPU)
  file(GLOB GE_GPU_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/gpu/*.cpp)
  add_library(ge_gpu STATIC ${GE_GPU_SOURCES})
  target_link_libraries(ge_gpu PUBLIC ge_core dawn::webgpu_dawn)
  ge_embed_wgsl(ge_gpu engine/gpu/test_scene.wgsl)
  # The frame core (M7): the pass table's mechanisms, the sky, the terrain and its horizon
  # map, the Hi-Z pyramid, TAA-U, post, the instance families and their meshes, the sun's shadows.
  file(GLOB GE_RENDER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/render/*.cpp)
  add_library(ge_render STATIC ${GE_RENDER_SOURCES})
  target_link_libraries(ge_render PUBLIC ge_gpu ge_assets)
  ge_embed_wgsl(ge_render engine/render/sky_tables.wgsl engine/render/sky.wgsl
    engine/render/terrain.wgsl engine/render/terrain_cull.wgsl engine/render/terrain_normals.wgsl
    engine/render/horizon.wgsl engine/render/hiz.wgsl engine/render/taa.wgsl engine/render/post.wgsl
    engine/render/instances.wgsl engine/render/instances_cull.wgsl engine/render/bloom.wgsl
    engine/render/skinning.wgsl)
endif()

# The network (engine/net, docs/networking.md): addresses, the wire, the OS's UDP sockets (the only
# file that includes them), the simulated network, captures, connections and nodes. No platform
# layer and no clock: the caller passes the time, so a session over the simulated network is
# deterministic.
file(GLOB GE_NET_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/net/*.cpp)
add_library(ge_net STATIC ${GE_NET_SOURCES})
target_link_libraries(ge_net PUBLIC ge_core)
if(WIN32)
  target_link_libraries(ge_net PRIVATE ws2_32)
endif()

# The lab framework (engine/lab: the contract, the runner, the raster, the PNG), published as
# ge::lab so a game's labs plug into the same contract as the engine's (docs/labs.md).
add_library(ge_lab STATIC engine/lab/lab.cpp engine/lab/report.cpp engine/lab/runner.cpp
  engine/lab/raster.cpp engine/lab/png.cpp engine/lab/diff.cpp engine/lab/inflate.cpp engine/lab/sound.cpp
  engine/lab/series.cpp engine/lab/stats.cpp engine/lab/plot.cpp engine/lab/chart.cpp
  engine/lab/anim.cpp)
target_link_libraries(ge_lab PUBLIC ge_core ge_audio)

# The profiler (engine/prof): the timing zones any code opens with GE_ZONE, the allocation
# counts and the resident set. The counting operator new is its own target, ge_prof_alloc: an
# executable that links it counts every allocation (the play executable does).
add_library(ge_prof STATIC engine/prof/zones.cpp engine/prof/memory.cpp)
target_link_libraries(ge_prof PUBLIC ge_core)
if(WIN32)
  target_link_libraries(ge_prof PRIVATE psapi)
endif()
add_library(ge_prof_alloc STATIC engine/prof/alloc_hook.cpp)
target_link_libraries(ge_prof_alloc PUBLIC ge_prof)

# The play contract (engine/play: a game's spec and Run, the session and its line protocol, the
# input log, replay and diverge, the bench, the stills, the `play` executable's main), published
# as ge::play so a game's `play` target is driven by `ge play`, `ge sim`, `ge replay`, `ge state`,
# `ge diverge`, `ge shot`, `ge bench`, `ge profile` and `ge mem`.
add_library(ge_play STATIC engine/play/inputs.cpp engine/play/json_read.cpp
  engine/play/session.cpp engine/play/bench.cpp engine/play/record.cpp engine/play/main.cpp)
target_link_libraries(ge_play PUBLIC ge_core ge_lab ge_prof ge_prof_alloc)

# The ge command line (engine/cli, cmake/Cli.cmake): built here too when a game pulls the engine
# in, so the game runs the ge of the engine it pins.
include(cmake/Cli.cmake)
ge_add_cli()

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
  target_link_libraries(labs PRIVATE ge_lab ge_play ge_core ge_audio ge_assets ge_net ${GE_PLUGIN_TARGETS} ${GE_GAME_TARGETS}
    ${GE_LAB_EXTRA})

  enable_testing()
  add_subdirectory(tests)
endif()

# Every embedded shader through Tint: `make shaders-check` (after every ge_embed_wgsl call).
if(GE_GPU AND GE_TINT)
  ge_add_shaders_check()
endif()

include(cmake/Packaging.cmake)
