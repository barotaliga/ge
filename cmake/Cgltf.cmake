# THE ASSET LOADER's dependency: cgltf (MIT), a single-file glTF 2.0 parser, downloaded as its
# one header from a pinned commit and checked against its SHA-256 (the assets skill). Only
# engine/assets/gltf.cpp includes it, and it compiles the implementation, so no public header
# of the engine names cgltf and a consumer of the package needs none of it.
#
# The header is downloaded once into GE_DEPS_DIR (default .cache/deps/, gitignored, shared by
# every preset and cached by CI). Its licence is the comment at the header's end.
#
# Defines: GE_CGLTF_INCLUDE, the directory that holds cgltf.h.
include_guard(GLOBAL)

# THE PIN. Moving it: change the tag, the commit and the hash together, then rerun
# tests/assets_gltf_test.cpp and the `models` lab.
set(GE_CGLTF_TAG "v1.15")
set(GE_CGLTF_COMMIT "360db1a95480fe102ae9c69b27c5d101167ff5ba")
set(GE_CGLTF_SHA256 "e378a21c084bf1f288bb799de827bb26906efb024255f1ecf1705ea13f11c6ec")

set(GE_DEPS_DIR "${PROJECT_SOURCE_DIR}/.cache/deps" CACHE PATH
  "Where prebuilt dependencies are downloaded and unpacked")
set(GE_CGLTF_INCLUDE "${GE_DEPS_DIR}/cgltf-${GE_CGLTF_TAG}")
set(_ge_cgltf_header "${GE_CGLTF_INCLUDE}/cgltf.h")
if(EXISTS "${_ge_cgltf_header}")
  file(SHA256 "${_ge_cgltf_header}" _ge_cgltf_have)
endif()
if(NOT EXISTS "${_ge_cgltf_header}" OR NOT _ge_cgltf_have STREQUAL GE_CGLTF_SHA256)
  message(STATUS "ge: downloading cgltf ${GE_CGLTF_TAG} into ${GE_CGLTF_INCLUDE}")
  file(DOWNLOAD
    "https://raw.githubusercontent.com/jkuhlmann/cgltf/${GE_CGLTF_COMMIT}/cgltf.h"
    "${_ge_cgltf_header}"
    EXPECTED_HASH SHA256=${GE_CGLTF_SHA256}
    STATUS _ge_cgltf_status)
  list(GET _ge_cgltf_status 0 _ge_cgltf_code)
  if(NOT _ge_cgltf_code EQUAL 0)
    list(GET _ge_cgltf_status 1 _ge_cgltf_why)
    file(REMOVE "${_ge_cgltf_header}")
    message(FATAL_ERROR "ge: could not download cgltf ${GE_CGLTF_TAG} (${_ge_cgltf_why}). "
      "Check the network, or place the header (SHA-256 ${GE_CGLTF_SHA256}) at "
      "${_ge_cgltf_header} by hand; skill: build.")
  endif()
endif()
