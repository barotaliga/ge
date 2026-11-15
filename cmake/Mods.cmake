# MODS AS BUILT (docs/mods.md, skill: mod): `ge_add_mod` makes a mod's shared library and, after
# every build, its package manifest beside it, so the output directory IS a package the game
# loads (engine/mod/loader.hpp). Used by this tree (the sample mod, the tests' fixtures) and by a
# mod repo that `ge mod new` writes, which includes this file from the engine tree it builds
# against. A mod links nothing of the engine: it compiles against engine/mod/abi.hpp and its
# game's header only, with the engine's determinism flags (cmake/Fp.cmake).
#
#   ge_add_mod(<target> NAME <kebab> VERSION <x.y.z> GAME <id> GAME_API <N|N..M>
#              SOURCES <files...> [INCLUDE_DIRS <dirs...>] [OUTPUT_DIR <dir>] [WASM])
#
# The library is <NAME with '-' as '_'>.so (.dll on Windows) in OUTPUT_DIR (default
# ${CMAKE_BINARY_DIR}/mods/<NAME>), with `mod.manifest` listing it for this platform with its
# SHA-256. The platform's name here must be engine/mod/manifest.cpp's `this_platform`: the sample
# mod's test loads what this file writes, so the two cannot drift apart unseen.
#
# WASM: the same sources also built for wasm32 (the store form, docs/mods.md § The store form),
# as <NAME with '-' as '_'>.wasm beside the library, by clang and wasm-ld with no C or C++ library
# (a mod needs none: abi.hpp's headers are the compiler's own). Only where both are found
# (GE_MOD_WASM_CLANG, GE_MOD_WASM_LD) and the
# compiler has the target (GE_MOD_WASM_WORKS); elsewhere nothing is built and the tests that run it skip.
#
# Run as a script (-P), this file writes that manifest: -DLIB -DOUT -DNAME -DVERSION -DGAME
# -DGAME_API -DABI -DPLATFORM.
if(CMAKE_SCRIPT_MODE_FILE)
  file(SHA256 "${LIB}" hash)
  get_filename_component(file "${LIB}" NAME)
  file(WRITE "${OUT}"
    "# A mod package (docs/mods.md § The package). Written by the build (cmake/Mods.cmake).\n"
    "name = ${NAME}\nversion = ${VERSION}\ngame = ${GAME}\ngame_api = ${GAME_API}\n"
    "abi = ${ABI}\nfile.${PLATFORM} = ${file} ${hash}\n")
  return()
endif()

include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/Fp.cmake")
find_program(GE_MOD_WASM_CLANG NAMES clang++ clang++-18 clang++-19 clang++-20)
find_program(GE_MOD_WASM_LD NAMES wasm-ld wasm-ld-18 wasm-ld-19 wasm-ld-20)
# A clang that has no wasm32 target (Apple's) is no toolchain: probed once, with a module of one
# exported function.
if(GE_MOD_WASM_CLANG AND GE_MOD_WASM_LD AND NOT DEFINED GE_MOD_WASM_WORKS)
  set(probe "${CMAKE_BINARY_DIR}/CMakeFiles/ge_mod_wasm_probe")
  file(WRITE "${probe}.cpp" "extern \"C\" __attribute__((visibility(\"default\"))) int f(int x) { return x; }\n")
  get_filename_component(ld_dir "${GE_MOD_WASM_LD}" DIRECTORY)
  execute_process(COMMAND "${GE_MOD_WASM_CLANG}" --target=wasm32 -O2 -nostdlib "-B${ld_dir}"
      -Wl,--no-entry -Wl,--export=f -o "${probe}.wasm" "${probe}.cpp"
    RESULT_VARIABLE probe_result OUTPUT_QUIET ERROR_QUIET)
  if(probe_result EQUAL 0)
    set(GE_MOD_WASM_WORKS ON CACHE INTERNAL "clang builds wasm32 modules")
  else()
    set(GE_MOD_WASM_WORKS OFF CACHE INTERNAL "clang builds wasm32 modules")
  endif()
endif()
function(ge_add_mod target)
  cmake_parse_arguments(M "WASM" "NAME;VERSION;GAME;GAME_API;OUTPUT_DIR" "SOURCES;INCLUDE_DIRS" ${ARGN})
  foreach(key NAME VERSION GAME GAME_API SOURCES)
    if(NOT M_${key})
      message(FATAL_ERROR "ge_add_mod(${target}): no ${key}; next: pass it (cmake/Mods.cmake "
        "lists the arguments)")
    endif()
  endforeach()
  # The engine tree, the ABI and the platform, worked out here (an include guard keeps a
  # directory-scope variable from reaching a second directory).
  get_filename_component(engine "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  # The ABI a mod built here speaks, read off engine/mod/abi.hpp (stated there once).
  file(STRINGS "${engine}/engine/mod/abi.hpp" abi REGEX "^#define GE_MOD_ABI_(MAJOR|MINOR) [0-9]+$")
  string(REGEX REPLACE ".*GE_MOD_ABI_MAJOR ([0-9]+).*" "\\1" abi_major "${abi}")
  string(REGEX REPLACE ".*GE_MOD_ABI_MINOR ([0-9]+).*" "\\1" abi_minor "${abi}")
  # This build's platform in the manifest's words: linux|macos|windows - x64|arm64.
  if(WIN32)
    set(os windows)
  elseif(APPLE)
    set(os macos)
  else()
    set(os linux)
  endif()
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(arch x64)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(arch arm64)
  else()
    set(arch "${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  if(NOT M_OUTPUT_DIR)
    set(M_OUTPUT_DIR "${CMAKE_BINARY_DIR}/mods/${M_NAME}")
  endif()
  string(REPLACE "-" "_" lib_name "${M_NAME}")
  add_library(${target} MODULE ${M_SOURCES})
  # A generator expression keeps a multi-config generator from adding a per-config directory.
  set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME "${lib_name}"
    LIBRARY_OUTPUT_DIRECTORY "$<1:${M_OUTPUT_DIR}>" RUNTIME_OUTPUT_DIRECTORY "$<1:${M_OUTPUT_DIR}>"
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
  if(APPLE)
    set_target_properties(${target} PROPERTIES SUFFIX ".so")
  endif()
  target_compile_features(${target} PRIVATE cxx_std_20)
  target_include_directories(${target} PRIVATE "${engine}/engine" ${M_INCLUDE_DIRS})
  target_link_libraries(${target} PRIVATE ge_fp ge_isa)
  if(TARGET ge_flags)
    target_link_libraries(${target} PRIVATE ge_flags)
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} "-DLIB=$<TARGET_FILE:${target}>" "-DOUT=${M_OUTPUT_DIR}/mod.manifest"
      "-DNAME=${M_NAME}" "-DVERSION=${M_VERSION}" "-DGAME=${M_GAME}" "-DGAME_API=${M_GAME_API}"
      "-DABI=${abi_major}.${abi_minor}" "-DPLATFORM=${os}-${arch}"
      -P "${engine}/cmake/Mods.cmake"
    VERBATIM)
  set_property(GLOBAL APPEND PROPERTY GE_MOD_TARGETS ${target})
  if(M_WASM AND GE_MOD_WASM_WORKS)
    set(wasm "${M_OUTPUT_DIR}/${lib_name}.wasm")
    set(includes "-I${engine}/engine")
    foreach(dir ${M_INCLUDE_DIRS})
      list(APPEND includes "-I${dir}")
    endforeach()
    get_filename_component(ld_dir "${GE_MOD_WASM_LD}" DIRECTORY)
    add_custom_command(OUTPUT "${wasm}"
      COMMAND "${GE_MOD_WASM_CLANG}" --target=wasm32 -std=c++20 -O2 -ffp-contract=off -nostdlib
        -fno-exceptions -fno-rtti -fvisibility=hidden ${includes} "-B${ld_dir}"
        -Wl,--no-entry -Wl,--export=ge_mod_entry -Wl,--export-table -Wl,--growable-table
        -o "${wasm}" ${M_SOURCES}
      DEPENDS ${M_SOURCES} "${engine}/engine/mod/abi.hpp"
      COMMENT "Building the mod ${M_NAME} for wasm32"
      VERBATIM)
    add_custom_target(${target}_wasm ALL DEPENDS "${wasm}")
    add_dependencies(${target}_wasm ${target})  # the package directory first
    set_property(GLOBAL APPEND PROPERTY GE_MOD_TARGETS ${target}_wasm)
  endif()
endfunction()
