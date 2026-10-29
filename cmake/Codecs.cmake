# THE EXPORT'S ENCODERS: AV1 by libaom (BSD-2-Clause and the AOMedia patent licence) and Opus by
# libopus (BSD-3-Clause), each downloaded as its release archive pinned by version and SHA-256,
# built once at configure time into GE_DEPS_DIR and linked statically into ge_play (the replay
# skill, docs/replays.md § Compressed video). engine/capture/codecs.cpp is the only code that
# includes either.
#
# Why AV1 and not VP9: libaom builds with CMake on every platform the engine ships on; libvpx
# builds only with its own configure script and make (an msys shell on Windows). Both are
# royalty-free and both play in a WebM file.
#
# GE_ENCODERS decides whether they are built. It is ON for the top-level build and OFF for a
# consumer that pulls the engine in, and the packaging and release workflows pass OFF, so the
# published package depends on neither. With it OFF, `--codec=av1` answers that this build has
# no encoders and the raw Matroska path is unchanged.
#
# The build is done once per version, platform and compiler and kept in GE_DEPS_DIR (cached by
# CI with Dawn and SDL), so every preset shares it. Building it takes about a minute.
#
# Defines, when GE_ENCODERS is ON: the imported targets ge::aom and ge::opus.
include_guard(GLOBAL)

option(GE_ENCODERS "Build the export's encoders (libaom's AV1, libopus's Opus)"
  ${PROJECT_IS_TOP_LEVEL})
if(NOT GE_ENCODERS)
  return()
endif()

# THE PINS. Moving one: change the version and the hash together, delete its directory under
# GE_DEPS_DIR, and rerun tests/capture_codecs_test.cpp and the capture lab.
set(GE_AOM_VERSION "3.12.1")
set(GE_AOM_SHA256 "9e9775180dec7dfd61a79e00bda3809d43891aee6b2e331ff7f26986207ea22e")
set(GE_OPUS_VERSION "1.5.2")
set(GE_OPUS_SHA256 "65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1")

enable_language(C)
set(GE_DEPS_DIR "${PROJECT_SOURCE_DIR}/.cache/deps" CACHE PATH
  "Where prebuilt dependencies are downloaded and unpacked")

# x86 assembly needs nasm (tools/setup-ubuntu.sh installs it). Without it libaom is built as
# plain C: the same bitstream, several times slower to encode.
find_program(GE_NASM nasm)
set(_ge_codec_cpu "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i.86)$" AND NOT GE_NASM)
  set(_ge_codec_cpu "generic")
  message(STATUS "ge: nasm not found; libaom is built without its x86 assembly (slower encodes)")
endif()

# One built copy per version, platform, compiler and CPU choice.
string(TOLOWER "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_C_COMPILER_ID}" _ge_codec_tag)
if(_ge_codec_cpu)
  string(APPEND _ge_codec_tag "-${_ge_codec_cpu}")
endif()

# ge_codec_build(<name> <version> <sha256> <url> <cmake args...>): download, unpack, configure,
# build and install one library into GE_DEPS_DIR, unless its stamp says it is already there.
# Sets GE_<NAME>_PREFIX.
function(ge_codec_build name version sha url)
  string(TOUPPER "${name}" upper)
  set(root "${GE_DEPS_DIR}/${name}-${version}")
  set(prefix "${root}/install-${_ge_codec_tag}")
  set(stamp "${prefix}/.ge-built")
  set(GE_${upper}_PREFIX "${prefix}" PARENT_SCOPE)
  if(EXISTS "${stamp}")
    return()
  endif()
  set(archive "${GE_DEPS_DIR}/${name}-${version}.tar.gz")
  message(STATUS "ge: building ${name} ${version} into ${prefix} (once; about a minute)")
  file(DOWNLOAD "${url}" "${archive}" EXPECTED_HASH SHA256=${sha} STATUS status)
  list(GET status 0 code)
  if(NOT code EQUAL 0)
    list(GET status 1 why)
    file(REMOVE "${archive}")
    message(FATAL_ERROR "ge: could not download ${name} ${version} (${why}). Check the network, "
      "place the archive (SHA-256 ${sha}) at ${archive} by hand, or configure with "
      "-DGE_ENCODERS=OFF to build without the encoders; skill: build.")
  endif()
  file(REMOVE_RECURSE "${root}/src" "${root}/build-${_ge_codec_tag}")
  file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${root}/src")
  file(GLOB source LIST_DIRECTORIES true "${root}/src/*")
  set(build "${root}/build-${_ge_codec_tag}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${build}" -G "${CMAKE_GENERATOR}"
      -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_INSTALL_PREFIX=${prefix}" -DCMAKE_INSTALL_LIBDIR=lib
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF
      -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
      ${ARGN}
    RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE log)
  if(result EQUAL 0)
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${build}" --config Release --parallel
      RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE log)
  endif()
  if(result EQUAL 0)
    execute_process(COMMAND "${CMAKE_COMMAND}" --install "${build}" --config Release
      RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE log)
  endif()
  if(NOT result EQUAL 0)
    file(WRITE "${root}/build-${_ge_codec_tag}.log" "${log}")
    message(FATAL_ERROR "ge: building ${name} ${version} failed; the log is "
      "${root}/build-${_ge_codec_tag}.log. Fix the toolchain it names, or configure with "
      "-DGE_ENCODERS=OFF; skill: build.")
  endif()
  file(REMOVE_RECURSE "${build}" "${root}/src")
  file(WRITE "${stamp}" "${version}\n")
endfunction()

set(_ge_aom_args -DCONFIG_REALTIME_ONLY=1 -DCONFIG_AV1_HIGHBITDEPTH=0 -DCONFIG_LIBYUV=0
  -DCONFIG_WEBM_IO=0 -DCONFIG_AV1_DECODER=1 -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTS=0
  -DENABLE_TESTDATA=0 -DENABLE_TOOLS=0)
if(GE_NASM)
  list(APPEND _ge_aom_args -DENABLE_NASM=1 "-DCMAKE_ASM_NASM_COMPILER=${GE_NASM}")
endif()
if(_ge_codec_cpu)
  list(APPEND _ge_aom_args -DAOM_TARGET_CPU=${_ge_codec_cpu})
endif()
ge_codec_build(aom ${GE_AOM_VERSION} ${GE_AOM_SHA256}
  "https://storage.googleapis.com/aom-releases/libaom-${GE_AOM_VERSION}.tar.gz" ${_ge_aom_args})
ge_codec_build(opus ${GE_OPUS_VERSION} ${GE_OPUS_SHA256}
  "https://github.com/xiph/opus/releases/download/v${GE_OPUS_VERSION}/opus-${GE_OPUS_VERSION}.tar.gz"
  -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_BUILD_TESTING=OFF -DOPUS_BUILD_SHARED_LIBRARY=OFF
  -DOPUS_INSTALL_PKG_CONFIG_MODULE=OFF -DOPUS_INSTALL_CMAKE_CONFIG_MODULE=OFF)

# The imported targets. libaom's static library needs the threads and, off Windows, libm.
find_library(GE_AOM_LIBRARY NAMES aom libaom PATHS "${GE_AOM_PREFIX}/lib" NO_DEFAULT_PATH REQUIRED)
find_library(GE_OPUS_LIBRARY NAMES opus libopus PATHS "${GE_OPUS_PREFIX}/lib" NO_DEFAULT_PATH
  REQUIRED)
add_library(ge::aom STATIC IMPORTED GLOBAL)
set_target_properties(ge::aom PROPERTIES IMPORTED_LOCATION "${GE_AOM_LIBRARY}")
target_include_directories(ge::aom SYSTEM INTERFACE "${GE_AOM_PREFIX}/include")
target_link_libraries(ge::aom INTERFACE Threads::Threads)
add_library(ge::opus STATIC IMPORTED GLOBAL)
set_target_properties(ge::opus PROPERTIES IMPORTED_LOCATION "${GE_OPUS_LIBRARY}")
target_include_directories(ge::opus SYSTEM INTERFACE "${GE_OPUS_PREFIX}/include")
if(NOT WIN32)
  target_link_libraries(ge::aom INTERFACE m)
  target_link_libraries(ge::opus INTERFACE m)
endif()
