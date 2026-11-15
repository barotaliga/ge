# THE ARCHIVES' CODECS (engine/vfs, the `vfs` skill): zstd (BSD-3-Clause, the licence we take of
# its BSD-or-GPLv2 pair) to pack recordings and read `.zst` files, and the LZMA SDK (public domain)
# to read `.xz` files and `.7z` archives, and bzip2 (its own BSD-style licence) to read `.bz2` files
# and zip entries of method 12, which downloaded recordings of the classic games often are. Each is downloaded as its release archive pinned by
# version and SHA-256 into GE_DEPS_DIR (default .cache/deps/, gitignored, cached by CI), unpacked
# once, and its C sources compiled into the object library ge_vfs_codecs, whose objects ge_vfs
# carries. So no public header of the engine names either library, a consumer of the package
# needs neither, and the tree's warnings and sanitizers (ge_flags) never meet their code.
#
# Only engine/vfs/zstd.cpp includes zstd, only engine/vfs/lzma.cpp the LZMA SDK and only
# engine/vfs/bzip2.cpp bzip2.
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
# - bzip2: the library's seven files without its command-line programs, built with
#   BZ_NO_STDIO (no file calls, errors come back as codes); the compressor writes the tests' files.
#
# Moving a pin: change the version and the hash together, then rerun tests/vfs_unpack_test.cpp,
# tests/vfs_zstd_test.cpp and the `se-demo-pack` lab.
#
# Defines: the object library ge_vfs_codecs and GE_ZSTD_INCLUDE, GE_LZMA_INCLUDE, GE_BZIP2_INCLUDE.
include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/FetchFile.cmake)
enable_language(C)

set(GE_ZSTD_VERSION "1.5.7")
set(GE_ZSTD_SHA256 "eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3")
set(GE_LZMA_VERSION "2501")
set(GE_LZMA_TAG "25.01")
set(GE_LZMA_SHA256 "cbc3babd589d971e45971d787ff100be8aaa5eab15b2694497ec3e447009e1f2")
set(GE_BZIP2_VERSION "1.0.8")
set(GE_BZIP2_SHA256 "ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269")

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

# The SDK from 7-Zip's own GitHub release (the same file as on its website, the same SHA-256):
# GitHub's hosts are the ones every runner already reaches, so one site down stops no build.
set(_ge_lzma_archive "${GE_DEPS_DIR}/lzma${GE_LZMA_VERSION}.7z")
ge_fetch_file("the LZMA SDK ${GE_LZMA_VERSION}"
  "https://github.com/ip7z/7zip/releases/download/${GE_LZMA_TAG}/lzma${GE_LZMA_VERSION}.7z"
  ${GE_LZMA_SHA256} "${_ge_lzma_archive}")
ge_archive_unpack("${_ge_lzma_archive}" "${GE_DEPS_DIR}/lzma${GE_LZMA_VERSION}" ${GE_LZMA_SHA256})
set(_ge_lzma_c "${GE_DEPS_DIR}/lzma${GE_LZMA_VERSION}/C")
set(GE_LZMA_INCLUDE "${_ge_lzma_c}")

set(_ge_bzip2_archive "${GE_DEPS_DIR}/bzip2-${GE_BZIP2_VERSION}.tar.gz")
ge_fetch_file("bzip2 ${GE_BZIP2_VERSION}"
  "https://sourceware.org/pub/bzip2/bzip2-${GE_BZIP2_VERSION}.tar.gz"
  ${GE_BZIP2_SHA256} "${_ge_bzip2_archive}")
ge_archive_unpack("${_ge_bzip2_archive}" "${GE_DEPS_DIR}/bzip2-${GE_BZIP2_VERSION}"
  ${GE_BZIP2_SHA256})
set(_ge_bzip2_dir "${GE_DEPS_DIR}/bzip2-${GE_BZIP2_VERSION}/bzip2-${GE_BZIP2_VERSION}")
set(GE_BZIP2_INCLUDE "${_ge_bzip2_dir}")
set(_ge_bzip2_sources)
foreach(f blocksort huffman crctable randtable compress decompress bzlib)
  list(APPEND _ge_bzip2_sources "${_ge_bzip2_dir}/${f}.c")
endforeach()

file(GLOB _ge_zstd_sources "${_ge_zstd_lib}/common/*.c" "${_ge_zstd_lib}/compress/*.c"
  "${_ge_zstd_lib}/decompress/*.c")
set(_ge_lzma_sources)
foreach(f 7zArcIn 7zBuf 7zCrc 7zCrcOpt 7zDec 7zStream Bcj2 Bra Bra86 BraIA64 CpuArch Delta
    Lzma2Dec LzmaDec Ppmd7 Ppmd7Dec Sha256 Sha256Opt Xz XzCrc64 XzCrc64Opt XzDec
    Alloc LzFind LzFindOpt LzmaEnc Lzma2Enc XzEnc)
  list(APPEND _ge_lzma_sources "${_ge_lzma_c}/${f}.c")
endforeach()

add_library(ge_vfs_codecs OBJECT ${_ge_zstd_sources} ${_ge_lzma_sources} ${_ge_bzip2_sources})
set_target_properties(ge_vfs_codecs PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
target_include_directories(ge_vfs_codecs PRIVATE "${_ge_zstd_lib}" "${_ge_zstd_lib}/common"
  "${_ge_lzma_c}" "${_ge_bzip2_dir}")
target_compile_definitions(ge_vfs_codecs PRIVATE ZSTD_DISABLE_ASM=1 ZSTD_LEGACY_SUPPORT=0
  ZSTD_MULTITHREAD_SUPPORT_DEFAULT=0 Z7_ST Z7_PPMD_SUPPORT BZ_NO_STDIO)
if(MSVC)
  target_compile_options(ge_vfs_codecs PRIVATE /w)
else()
  target_compile_options(ge_vfs_codecs PRIVATE -w)
endif()

# unRAR (its own licence: any program may extract RAR archives with it; only rebuilding the
# compressor is forbidden), fetched at build time and never committed; engine/vfs/rar.cpp alone
# includes it.
set(GE_UNRAR_VERSION "7.3.2")
set(GE_UNRAR_SHA256 "f61a1160d6e2ebbda2ad353c8e82f62ab4312f97fde23c6c2c982d25e0014f99")

set(_ge_unrar_archive "${GE_DEPS_DIR}/unrarsrc-${GE_UNRAR_VERSION}.tar.gz")
ge_fetch_file("unRAR ${GE_UNRAR_VERSION}"
  "https://www.rarlab.com/rar/unrarsrc-${GE_UNRAR_VERSION}.tar.gz"
  ${GE_UNRAR_SHA256} "${_ge_unrar_archive}")
ge_archive_unpack("${_ge_unrar_archive}" "${GE_DEPS_DIR}/unrar-${GE_UNRAR_VERSION}"
  ${GE_UNRAR_SHA256})
set(_ge_unrar_dir "${GE_DEPS_DIR}/unrar-${GE_UNRAR_VERSION}/unrar")
set(GE_UNRAR_INCLUDE "${_ge_unrar_dir}")
set(_ge_unrar_sources)
foreach(f rar strlist strfn pathfn smallfn global file filefn filcreat archive arcread unicode
    system crypt crc rawread encname resource match timefn rdwrfn consio options errhnd rarvm
    secpassword rijndael getbits sha1 sha256 blake2s hash extinfo extract volume list find unpack
    headers threadpool rs16 cmddata ui largepage filestr scantree dll qopen)
  list(APPEND _ge_unrar_sources "${_ge_unrar_dir}/${f}.cpp")
endforeach()
if(WIN32)
  list(APPEND _ge_unrar_sources "${_ge_unrar_dir}/isnt.cpp" "${_ge_unrar_dir}/motw.cpp")
endif()
add_library(ge_vfs_unrar OBJECT ${_ge_unrar_sources})
set_target_properties(ge_vfs_unrar PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 20)
target_include_directories(ge_vfs_unrar PRIVATE "${_ge_unrar_dir}")
target_compile_definitions(ge_vfs_unrar PRIVATE RARDLL UNRAR SILENT)
if(MSVC)
  target_compile_options(ge_vfs_unrar PRIVATE /w)
  target_compile_definitions(ge_vfs_unrar PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(ge_vfs_unrar PRIVATE -w)
  target_compile_definitions(ge_vfs_unrar PRIVATE _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE)
endif()
