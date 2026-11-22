# TLS FOR THE WEB'S SECURE ADDRESSES: Mbed TLS (Apache-2.0, the licence we take of its
# Apache-or-GPLv2 pair), downloaded as its release archive pinned by version and SHA-256 into
# GE_DEPS_DIR (default .cache/deps/, gitignored, cached by CI) and unpacked once. Its C sources
# (crypto, X.509 and TLS, with the generated files the release carries) are compiled into the
# object library ge_mbedtls, whose objects ge_net carries: engine/net/tls_stream.cpp is the only
# code that includes it, so no public header of the engine names it and a consumer of the package
# needs nothing of it. Built with the release's default configuration (TLS 1.2 and 1.3, the
# certificate chain checked), its warnings off; the tree's warnings and sanitizers never meet it.
#
# Why Mbed TLS: one library with the same certificate checks on every platform the engine builds
# for, small enough to carry inside ge_net; the trusted roots are the operating system's
# (engine/net/tls_stream.hpp says where each is read from).
#
# Moving the pin: change the version and the hash together, then rerun
# tests/net_tls_stream_test.cpp and tests/net_http_fetch_test.cpp.
#
# Defines: the object library ge_mbedtls and GE_MBEDTLS_INCLUDE.
include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/FetchFile.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/Archives.cmake)  # ge_archive_unpack
enable_language(C)

set(GE_MBEDTLS_VERSION "3.6.5")
set(GE_MBEDTLS_SHA256 "4a11f1777bb95bf4ad96721cac945a26e04bf19f57d905f241fe77ebeddf46d8")

set(_ge_mbedtls_archive "${GE_DEPS_DIR}/mbedtls-${GE_MBEDTLS_VERSION}.tar.bz2")
ge_fetch_file("Mbed TLS ${GE_MBEDTLS_VERSION}"
  "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-${GE_MBEDTLS_VERSION}/mbedtls-${GE_MBEDTLS_VERSION}.tar.bz2"
  ${GE_MBEDTLS_SHA256} "${_ge_mbedtls_archive}")
ge_archive_unpack("${_ge_mbedtls_archive}" "${GE_DEPS_DIR}/mbedtls-${GE_MBEDTLS_VERSION}-src"
  ${GE_MBEDTLS_SHA256})
set(_ge_mbedtls_root "${GE_DEPS_DIR}/mbedtls-${GE_MBEDTLS_VERSION}-src/mbedtls-${GE_MBEDTLS_VERSION}")
set(GE_MBEDTLS_INCLUDE "${_ge_mbedtls_root}/include")

file(GLOB _ge_mbedtls_sources "${_ge_mbedtls_root}/library/*.c")
# The library's own sockets and timing are not used (the engine's sockets carry its bytes).
list(FILTER _ge_mbedtls_sources EXCLUDE REGEX "/(net_sockets|timing)\\.c$")
add_library(ge_mbedtls OBJECT ${_ge_mbedtls_sources})
target_include_directories(ge_mbedtls PRIVATE "${GE_MBEDTLS_INCLUDE}" "${_ge_mbedtls_root}/library")
target_compile_options(ge_mbedtls PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)
set_target_properties(ge_mbedtls PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
