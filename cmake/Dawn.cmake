# THE GPU BACKEND's dependency: Dawn (the WebGPU API, webgpu.h) as its prebuilt GitHub release,
# pinned to one tag, and Tint (`tint`, its shader compiler) from the same archive for
# `make shaders-check`. docs/rendering.md § The backend is the decision; the gpu skill is the
# procedure for moving the pin.
#
# GE_GPU decides whether engine/gpu (and everything that draws) is built. It defaults ON where
# a prebuilt archive exists for the host (Linux x64, macOS arm64, Windows x64) and OFF elsewhere
# (Linux arm64, until Dawn is built from source there: milestone M11). With it OFF the rest of
# the engine builds and tests unchanged.
#
# The archive is downloaded once into GE_DEPS_DIR (default .cache/deps/, gitignored, shared by
# every preset and cached by CI) and checked against its SHA-256. GE_DAWN_DIR names an
# unpacked Dawn install instead (a local source build, or a mirror), skipping the download.
#
# Defines, when GE_GPU is ON: the imported target dawn::webgpu_dawn, and GE_TINT (the path of
# the tint executable).
include_guard(GLOBAL)

# THE PIN. Moving it: pick a tag whose release has all three archives, update the tag, the
# commit and the three hashes together, rebuild, and rerun every gpu test and lab (gpu skill).
set(GE_DAWN_TAG "v20261016.190333")
set(GE_DAWN_COMMIT "67d4d6e64b0755d638f76549794692badfdbc8ab")
set(GE_DAWN_SHA256_ubuntu-latest "20ff88448def9645ceeff30911e07295aeb433be23e26180c96019b4300bff5a")
set(GE_DAWN_SHA256_macos-latest "736fccba78b8a2fde8bfd55d045e890933b0a01776aed5a29b9f024735404193")
set(GE_DAWN_SHA256_windows-latest "57402c6aa887e30d3e5710707d6b0677d8292538ab941628c624635022bfa49f")

# Which archive serves this host, if any.
set(ge_dawn_os "")
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ge_cpu)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND ge_cpu MATCHES "^(x86_64|amd64)$")
  set(ge_dawn_os "ubuntu-latest")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND ge_cpu MATCHES "^(arm64|aarch64)$")
  set(ge_dawn_os "macos-latest")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND ge_cpu MATCHES "^(x86_64|amd64)$")
  set(ge_dawn_os "windows-latest")
endif()

if(ge_dawn_os OR GE_DAWN_DIR)
  set(ge_gpu_default ON)
else()
  set(ge_gpu_default OFF)
endif()
option(GE_GPU "Build engine/gpu and everything that draws (needs Dawn)" ${ge_gpu_default})
set(GE_DEPS_DIR "${PROJECT_SOURCE_DIR}/.cache/deps" CACHE PATH
  "Where prebuilt dependencies are downloaded and unpacked")
set(GE_DAWN_DIR "" CACHE PATH "An unpacked Dawn install to use instead of the pinned download")

if(NOT GE_GPU)
  return()
endif()

if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(FATAL_ERROR "ge: GE_GPU=ON with an MSVC Debug build: the prebuilt Dawn is a Release "
    "library and its C++ runtime does not mix with a Debug one. Build the release preset "
    "(PRESET=release), or configure with -DGE_GPU=OFF; skill: build.")
endif()

if(NOT GE_DAWN_DIR)
  if(NOT ge_dawn_os)
    message(FATAL_ERROR "ge: GE_GPU=ON but Dawn publishes no prebuilt archive for "
      "${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}. Configure with -DGE_GPU=OFF, or build "
      "Dawn from source and pass -DGE_DAWN_DIR=<its install prefix>; skill: gpu.")
  endif()
  set(ge_dawn_name "Dawn-${GE_DAWN_COMMIT}-${ge_dawn_os}-Release")
  set(GE_DAWN_DIR "${GE_DEPS_DIR}/${ge_dawn_name}")
  if(NOT EXISTS "${GE_DAWN_DIR}/include/webgpu/webgpu.h")
    set(archive "${GE_DEPS_DIR}/${ge_dawn_name}.tar.gz")
    set(url "https://github.com/google/dawn/releases/download/${GE_DAWN_TAG}/${ge_dawn_name}.tar.gz")
    message(STATUS "ge: downloading Dawn ${GE_DAWN_TAG} (${ge_dawn_os}) into ${GE_DEPS_DIR}")
    file(DOWNLOAD "${url}" "${archive}" EXPECTED_HASH SHA256=${GE_DAWN_SHA256_${ge_dawn_os}}
      STATUS status TLS_VERIFY ON)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
      list(GET status 1 why)
      file(REMOVE "${archive}")
      message(FATAL_ERROR "ge: downloading ${url} failed (${why}). Check the network, or pass "
        "-DGE_DAWN_DIR=<an unpacked copy of that archive>, or -DGE_GPU=OFF; skill: gpu.")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${GE_DEPS_DIR}")
    file(REMOVE "${archive}")
  endif()
endif()

file(GLOB ge_dawn_config "${GE_DAWN_DIR}/lib*/cmake/Dawn/DawnConfig.cmake")
if(NOT ge_dawn_config)
  message(FATAL_ERROR "ge: no lib*/cmake/Dawn/DawnConfig.cmake under GE_DAWN_DIR="
    "${GE_DAWN_DIR}. It must be a Dawn install prefix (include/, lib/ and bin/); skill: gpu.")
endif()
list(GET ge_dawn_config 0 ge_dawn_config)
get_filename_component(Dawn_DIR "${ge_dawn_config}" DIRECTORY)
find_package(Threads REQUIRED)
find_package(Dawn CONFIG REQUIRED NO_DEFAULT_PATH PATHS "${Dawn_DIR}")
find_program(GE_TINT NAMES tint PATHS "${GE_DAWN_DIR}/bin" NO_DEFAULT_PATH)
