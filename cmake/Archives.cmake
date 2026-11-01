# THE ARCHIVES' CODECS (engine/vfs, the `vfs` skill): zstd (BSD-3-Clause, the licence we take of
# its BSD-or-GPLv2 pair) to pack recordings and read `.zst` files, and the LZMA SDK (public domain)
# to read `.xz` files and `.7z` archives. Each is downloaded as its release archive pinned by
# version and SHA-256 into GE_DEPS_DIR (default .cache/deps/, gitignored, cached by CI), unpacked
# once, and its C sources compiled into the object library ge_vfs_codecs, whose objects ge_vfs
# carries. So no public header of the engine names either library, a consumer of the package
# needs neither, and the tree's warnings and sanitizers (ge_flags) never meet their code.
#
# Only engine/vfs/zstd.cpp includes zstd and only engine/vfs/lzma.cpp the LZMA SDK.
#
# What is compiled, and why not more:
# - zstd: lib/common, lib/compress and lib/decompress, single-threaded, without the x86 assembly
#   of the Huffman decoder (ZSTD_DISABLE_ASM: one portable C path on every compiler; the lab
#   `se-demo-pack` measures the decode speed it leaves), without the legacy formats (before 0.8)
#   and without the dictionary builder (recordings are megabytes, where a dictionary gains
#   nothing: docs/goals/M23j.md has the measurement).
# - the LZMA SDK: the decoders a 7z archive or an xz stream may name (LZMA, LZMA2, PPMd, the
#   branch filters, BCJ2, delta), the 7z archive reader and the xz unpacker, and the LZMA and xz
#   encoders, all single-threaded (Z7_ST). The encoders write the tests' and the labs' `.7z` and
#   `.xz` files and are the lab's LZMA rows against zstd; a game that never calls
#   engine/vfs/lzma.hpp's writers links none of them (the linker takes only objects it needs).
#
# Moving a pin: change the version and the hash together, then rerun tests/vfs_unpack_test.cpp,
# tests/vfs_zstd_test.cpp and the `se-demo-pack` lab.
#
# Defines: the object library ge_vfs_codecs and GE_ZSTD_INCLUDE, GE_LZMA_INCLUDE.
include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/FetchFile.cmake)
enable_language(C)

set(GE_ZSTD_VERSION "1.5.7")
set(GE_ZSTD_SHA256 "eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3")
set(GE_LZMA_VERSION "2501")
set(GE_LZMA_SHA256 "cbc3babd589d971e45971d787ff100be8aaa5eab15b2694497ec3e447009e1f2")

# ge_archive_unpack(<archive> <dir>): the archive's contents in <dir>, unpacked once (a stamp
# holds the archive's hash, so a moved pin unpacks again).
function(ge_archive_unpack archive dir sha)
  set(stamp "${dir}/.ge-unpacked")
  if(EXISTS "${stamp}")
    file(READ "${stamp}" have)
    if(have STREQUAL sha)
      return()
    endif()
  endif()
  file(REMOVE_RECURSE "${dir}")
  file(MAKE_DIRECTORY "${dir}")
  file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${dir}")
  file(WRITE "${stamp}" "${sha}")
endfunction()

set(_ge_zstd_archive "${GE_DEPS_DIR}/zstd-${GE_ZSTD_VERSION}.tar.gz")
ge_fetch_file("zstd ${GE_ZSTD_VERSION}"
  "https://github.com/facebook/zstd/releases/download/v${GE_ZSTD_VERSION}/zstd-${GE_ZSTD_VERSION}.tar.gz"
  ${GE_ZSTD_SHA256} "${_ge_zstd_archive}")
ge_archive_unpack("${_ge_zstd_archive}" "${GE_DEPS_DIR}/zstd-${GE_ZSTD_VERSION}" ${GE_ZSTD_SHA256})
set(_ge_zstd_lib "${GE_DEPS_DIR}/zstd-${GE_ZSTD_VERSION}/zstd-${GE_ZSTD_VERSION}/lib")
set(GE_ZSTD_INCLUDE "${_ge_zstd_lib}")

set(_ge_lzma_archive "${GE_DEPS_DIR}/lzma${GE_LZMA_VERSION}.7z")
ge_fetch_file("the LZMA SDK ${GE_LZMA_VERSION}"
  "https://www.7-zip.org/a/lzma${GE_LZMA_VERSION}.7z" ${GE_LZMA_SHA256} "${_ge_lzma_archive}")
ge_archive_unpack("${_ge_lzma_archive}" "${GE_DEPS_DIR}/lzma${GE_LZMA_VERSION}" ${GE_LZMA_SHA256})
set(_ge_lzma_c "${GE_DEPS_DIR}/lzma${GE_LZMA_VERSION}/C")
set(GE_LZMA_INCLUDE "${_ge_lzma_c}")

file(GLOB _ge_zstd_sources "${_ge_zstd_lib}/common/*.c" "${_ge_zstd_lib}/compress/*.c"
  "${_ge_zstd_lib}/decompress/*.c")
set(_ge_lzma_sources)
foreach(f 7zArcIn 7zBuf 7zCrc 7zCrcOpt 7zDec 7zStream Bcj2 Bra Bra86 BraIA64 CpuArch Delta
    Lzma2Dec LzmaDec Ppmd7 Ppmd7Dec Sha256 Sha256Opt Xz XzCrc64 XzCrc64Opt XzDec
    Alloc LzFind LzFindOpt LzmaEnc Lzma2Enc XzEnc)
  list(APPEND _ge_lzma_sources "${_ge_lzma_c}/${f}.c")
endforeach()

add_library(ge_vfs_codecs OBJECT ${_ge_zstd_sources} ${_ge_lzma_sources})
set_target_properties(ge_vfs_codecs PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
target_include_directories(ge_vfs_codecs PRIVATE "${_ge_zstd_lib}" "${_ge_zstd_lib}/common"
  "${_ge_lzma_c}")
target_compile_definitions(ge_vfs_codecs PRIVATE ZSTD_DISABLE_ASM=1 ZSTD_LEGACY_SUPPORT=0
  ZSTD_MULTITHREAD_SUPPORT_DEFAULT=0 Z7_ST Z7_PPMD_SUPPORT)
if(MSVC)
  target_compile_options(ge_vfs_codecs PRIVATE /w)
else()
  target_compile_options(ge_vfs_codecs PRIVATE -w)
endif()
