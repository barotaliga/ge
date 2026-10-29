# THE NETWORK'S CRYPTOGRAPHY: Monocypher (BSD-2-Clause or CC0, the licence is in its archive), a
# small C library of modern primitives, downloaded as its release archive pinned by version and
# SHA-256 (the net skill, docs/networking.md § Security). engine/net/crypto.cpp is the only code
# that includes it: XChaCha20-Poly1305 seals the connect tokens and every payload, and keyed
# BLAKE2b derives each host's key from the matchmaker's.
#
# Its one source file is compiled as C into an object library whose objects go into ge_net, so
# the published package carries it inside ge_net and a consumer needs nothing of it. Integer
# arithmetic only: what it seals is the same bytes on every platform, so a secure session over the
# simulated network keeps its digest.
#
# The archive is downloaded once into GE_DEPS_DIR (default .cache/deps/, gitignored, shared by
# every preset and cached by CI) and unpacked beside it.
#
# Defines: the object library ge_monocypher and GE_MONOCYPHER_INCLUDE (the directory of
# monocypher.h).
include_guard(GLOBAL)
include(cmake/FetchFile.cmake)

# THE PIN. Moving it: change the version and the hash together, delete its directory under
# GE_DEPS_DIR, then rerun tests/net_crypto_test.cpp (the RFC vectors) and the net-link lab with
# --secure.
set(GE_MONOCYPHER_VERSION "4.0.2")
set(GE_MONOCYPHER_SHA256 "38d07179738c0c90677dba3ceb7a7b8496bcfea758ba1a53e803fed30ae0879c")

set(_ge_mc_name "monocypher-${GE_MONOCYPHER_VERSION}")
set(_ge_mc_archive "${GE_DEPS_DIR}/${_ge_mc_name}.tar.gz")
ge_fetch_file("Monocypher ${GE_MONOCYPHER_VERSION}"
  "https://github.com/LoupVaillant/Monocypher/releases/download/${GE_MONOCYPHER_VERSION}/${_ge_mc_name}.tar.gz"
  ${GE_MONOCYPHER_SHA256} "${_ge_mc_archive}")
set(GE_MONOCYPHER_INCLUDE "${GE_DEPS_DIR}/${_ge_mc_name}/src")
set(_ge_mc_stamp "${GE_DEPS_DIR}/${_ge_mc_name}/.unpacked-${GE_MONOCYPHER_SHA256}")
if(NOT EXISTS "${_ge_mc_stamp}")
  file(ARCHIVE_EXTRACT INPUT "${_ge_mc_archive}" DESTINATION "${GE_DEPS_DIR}")
  file(TOUCH "${_ge_mc_stamp}")
endif()

enable_language(C)
add_library(ge_monocypher OBJECT "${GE_MONOCYPHER_INCLUDE}/monocypher.c")
set_target_properties(ge_monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
target_include_directories(ge_monocypher PRIVATE "${GE_MONOCYPHER_INCLUDE}")
# Kept as released: its warnings are not ours to quiet line by line.
if(MSVC)
  target_compile_options(ge_monocypher PRIVATE /W0)
else()
  target_compile_options(ge_monocypher PRIVATE -w)
endif()
