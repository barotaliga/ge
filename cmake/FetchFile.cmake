# One pinned file downloaded into GE_DEPS_DIR and checked against its SHA-256:
# `ge_fetch_file(<what> <url> <sha256> <dest>)`. A file already there with the right hash is
# kept, so a build downloads each file once (CI caches GE_DEPS_DIR). Used for the single-header
# and single-file dependencies that have no release archive (the UI's font rasterizer, its fonts).
include_guard(GLOBAL)

set(GE_DEPS_DIR "${PROJECT_SOURCE_DIR}/.cache/deps" CACHE PATH
  "Where prebuilt dependencies are downloaded and unpacked")

function(ge_fetch_file what url sha256 dest)
  if(EXISTS "${dest}")
    file(SHA256 "${dest}" have)
    if(have STREQUAL sha256)
      return()
    endif()
  endif()
  message(STATUS "ge: downloading ${what} into ${dest}")
  file(DOWNLOAD "${url}" "${dest}" EXPECTED_HASH SHA256=${sha256} STATUS status)
  list(GET status 0 code)
  if(NOT code EQUAL 0)
    list(GET status 1 why)
    file(REMOVE "${dest}")
    message(FATAL_ERROR "ge: could not download ${what} (${why}) from ${url}. Check the network, "
      "or place the file (SHA-256 ${sha256}) at ${dest} by hand; skill: build.")
  endif()
endfunction()
