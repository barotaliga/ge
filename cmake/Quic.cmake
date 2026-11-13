# QUIC AND ITS TLS, for the WebTransport endpoint (engine/webtransport, docs/hub.md § Playing in a
# browser): ngtcp2 (MIT) speaks QUIC with RFC 9221 datagrams, and picotls (MIT) with its own
# portable cryptography, "minicrypto" (cifra, CC0, and micro-ecc, BSD-2-Clause, both carried in
# picotls's tree), does the TLS 1.3 handshake under it. Nothing of the OS's: no OpenSSL, so the
# same objects build on Linux, macOS and Windows. Chosen over the other QUIC stacks because these
# two are small C libraries with permissive licences and no system dependency.
#
# ngtcp2 comes as its release archive pinned by version and SHA-256; picotls has no releases, so it
# is cloned at a pinned commit. Both land once in GE_DEPS_DIR (default .cache/deps/, gitignored,
# shared by every preset). Their C files are compiled into one object library whose objects go into
# ge_webtransport, so the published package carries them and a consumer needs nothing of them.
#
# ngtcp2's own picotls glue names picotls's OpenSSL backend; a header written here maps those names
# onto minicrypto's (the same algorithms), so the glue builds unchanged.
#
# Defines: the object library ge_quic, GE_QUIC_INCLUDES (ngtcp2's, its crypto's and picotls's
# public headers, and micro-ecc's for the certificate's signature) and GE_QUIC_DEFINES (what code
# that includes ngtcp2.h compiles with).
include_guard(GLOBAL)
include(cmake/FetchFile.cmake)

# THE PINS. Moving one: change the version (or commit) and the hash together, delete its directory
# under GE_DEPS_DIR, then rerun tests/webtransport_*_test.cpp and the webtransport lab.
set(GE_NGTCP2_VERSION "1.21.0")
set(GE_NGTCP2_SHA256 "2d1c07e6aa509c017516c08307b0b707cd165a17275ab5f1caff9aaa0e3b6c7d")
set(GE_PICOTLS_COMMIT "7f0d38c1c54c920d56b4e13ceb1c2dfa2b1325e1")

set(_ge_ng "${GE_DEPS_DIR}/ngtcp2-${GE_NGTCP2_VERSION}")
set(_ge_ng_archive "${_ge_ng}.tar.xz")
ge_fetch_file("ngtcp2 ${GE_NGTCP2_VERSION}"
  "https://github.com/ngtcp2/ngtcp2/releases/download/v${GE_NGTCP2_VERSION}/ngtcp2-${GE_NGTCP2_VERSION}.tar.xz"
  ${GE_NGTCP2_SHA256} "${_ge_ng_archive}")
set(_ge_ng_stamp "${_ge_ng}/.unpacked-${GE_NGTCP2_SHA256}")
if(NOT EXISTS "${_ge_ng_stamp}")
  file(ARCHIVE_EXTRACT INPUT "${_ge_ng_archive}" DESTINATION "${GE_DEPS_DIR}")
  file(TOUCH "${_ge_ng_stamp}")
endif()

set(_ge_pt "${GE_DEPS_DIR}/picotls-${GE_PICOTLS_COMMIT}")
if(NOT EXISTS "${_ge_pt}/.checked-out")
  find_package(Git REQUIRED)
  file(REMOVE_RECURSE "${_ge_pt}")
  message(STATUS "ge: cloning picotls ${GE_PICOTLS_COMMIT} into ${_ge_pt}")
  execute_process(COMMAND "${GIT_EXECUTABLE}" clone --quiet https://github.com/h2o/picotls.git
                          "${_ge_pt}" RESULT_VARIABLE _ge_pt_rc)
  if(_ge_pt_rc EQUAL 0)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${_ge_pt}" checkout --quiet ${GE_PICOTLS_COMMIT}
                    RESULT_VARIABLE _ge_pt_rc)
  endif()
  if(NOT _ge_pt_rc EQUAL 0)
    file(REMOVE_RECURSE "${_ge_pt}")
    message(FATAL_ERROR "ge: could not clone picotls at ${GE_PICOTLS_COMMIT} from "
      "https://github.com/h2o/picotls.git. Check the network and git, then configure again; "
      "skill: build.")
  endif()
  file(TOUCH "${_ge_pt}/.checked-out")
endif()

# ngtcp2's configuration header, per platform (its CMake build would probe the same).
set(_ge_quic_gen "${CMAKE_BINARY_DIR}/ge_quic")
set(_ge_cfg "#pragma once\n#define NGTCP2_STATICLIB 1\n")
if(WIN32)
  string(APPEND _ge_cfg "#define HAVE_DECL_BE64TOH 0\n#define HAVE_DECL_BSWAP_64 0\n"
    "#ifndef WIN32_LEAN_AND_MEAN\n#define WIN32_LEAN_AND_MEAN\n#endif\n#include <windows.h>\n")
elseif(APPLE)
  string(APPEND _ge_cfg "#define HAVE_ARPA_INET_H 1\n#define HAVE_NETINET_IN_H 1\n"
    "#define HAVE_UNISTD_H 1\n#define HAVE_DECL_BE64TOH 0\n#define HAVE_DECL_BSWAP_64 0\n"
    "#define HAVE_MEMSET_S 1\n")
else()
  string(APPEND _ge_cfg "#define HAVE_ARPA_INET_H 1\n#define HAVE_NETINET_IN_H 1\n"
    "#define HAVE_UNISTD_H 1\n#define HAVE_ENDIAN_H 1\n#define HAVE_BYTESWAP_H 1\n"
    "#define HAVE_DECL_BE64TOH 1\n#define HAVE_DECL_BSWAP_64 1\n#define HAVE_EXPLICIT_BZERO 1\n")
endif()
file(CONFIGURE OUTPUT "${_ge_quic_gen}/config.h" CONTENT "${_ge_cfg}")
# The OpenSSL backend's names, as minicrypto's.
file(CONFIGURE OUTPUT "${_ge_quic_gen}/picotls/openssl.h" CONTENT [=[
#pragma once
#include "picotls/minicrypto.h"
#define PTLS_OPENSSL_HAVE_CHACHA20_POLY1305 1
#define ptls_openssl_aes128gcm ptls_minicrypto_aes128gcm
#define ptls_openssl_aes256gcm ptls_minicrypto_aes256gcm
#define ptls_openssl_chacha20poly1305 ptls_minicrypto_chacha20poly1305
#define ptls_openssl_sha256 ptls_minicrypto_sha256
#define ptls_openssl_aes128ctr ptls_minicrypto_aes128ctr
#define ptls_openssl_aes256ctr ptls_minicrypto_aes256ctr
#define ptls_openssl_chacha20 ptls_minicrypto_chacha20
#define ptls_openssl_random_bytes ptls_minicrypto_random_bytes
]=])

file(GLOB _ge_ng_lib "${_ge_ng}/lib/*.c")
set(_ge_pt_cifra aes blockwise chacha20 chash curve25519 drbg hmac gcm gf128 modes poly1305 sha256
  sha512)
list(TRANSFORM _ge_pt_cifra PREPEND "${_ge_pt}/deps/cifra/src/")
list(TRANSFORM _ge_pt_cifra APPEND ".c")

enable_language(C)
add_library(ge_quic OBJECT
  ${_ge_ng_lib}
  "${_ge_ng}/crypto/shared.c"
  "${_ge_ng}/crypto/picotls/picotls.c"
  "${_ge_pt}/lib/picotls.c" "${_ge_pt}/lib/hpke.c" "${_ge_pt}/lib/pembase64.c"
  "${_ge_pt}/lib/cifra.c" "${_ge_pt}/lib/uecc.c" "${_ge_pt}/lib/cifra/x25519.c"
  "${_ge_pt}/lib/cifra/chacha20.c" "${_ge_pt}/lib/cifra/aes128.c" "${_ge_pt}/lib/cifra/aes256.c"
  "${_ge_pt}/lib/cifra/random.c"
  "${_ge_pt}/deps/micro-ecc/uECC.c"
  ${_ge_pt_cifra})
set_target_properties(ge_quic PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 11)
set(GE_QUIC_INCLUDES
  "${_ge_ng}/lib/includes" "${_ge_ng}/crypto/includes" "${_ge_pt}/include"
  "${_ge_pt}/deps/micro-ecc")
target_include_directories(ge_quic PRIVATE "${_ge_quic_gen}" ${GE_QUIC_INCLUDES}
  "${_ge_ng}/lib" "${_ge_ng}/crypto" "${_ge_pt}/deps/cifra/src" "${_ge_pt}/deps/cifra/src/ext")
# ngtcp2's own socket address types (no OS header in the endpoint: engine/net owns those).
set(GE_QUIC_DEFINES NGTCP2_STATICLIB=1 NGTCP2_USE_GENERIC_SOCKADDR=1 NGTCP2_AF_INET=2
  NGTCP2_AF_INET6=10)
target_compile_definitions(ge_quic PRIVATE HAVE_CONFIG_H=1 ${GE_QUIC_DEFINES})
if(WIN32)
  # picotls's clock on Windows; windows.h (in config.h) is what ngtcp2 wipes its keys with.
  target_sources(ge_quic PRIVATE "${_ge_pt}/picotlsvs/picotls/wintimeofday.c")
  target_include_directories(ge_quic PRIVATE "${_ge_pt}/picotlsvs/picotls")
  target_compile_definitions(ge_quic PRIVATE _WINDOWS=1 _CRT_SECURE_NO_WARNINGS=1)
endif()
# Kept as released: their warnings are not ours to quiet line by line.
if(MSVC)
  target_compile_options(ge_quic PRIVATE /W0)
else()
  target_compile_options(ge_quic PRIVATE -w)
endif()
