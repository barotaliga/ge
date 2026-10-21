# A game consuming an INSTALLED engine package, the way a release is used.
# The release workflow and CI build it against `cmake --install` output:
#   cmake -S tests/package/consumer -B build-consumer -DCMAKE_PREFIX_PATH=<stage>
# It proves the package resolves, its version matches VERSION and the
# exported targets compile and link with every installed header. Not part of the engine's own build.
cmake_minimum_required(VERSION 3.25)
project(ge_package_consumer LANGUAGES CXX)

if(NOT DEFINED GE_EXPECT_VERSION)
  file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../../VERSION" GE_EXPECT_VERSION LIMIT_COUNT 1)
endif()
string(REGEX MATCH "^[0-9]+\\.[0-9]+" _ge_minor "${GE_EXPECT_VERSION}")

find_package(gameengine2 ${_ge_minor} REQUIRED CONFIG)
if(NOT gameengine2_VERSION VERSION_EQUAL GE_EXPECT_VERSION)
  message(FATAL_ERROR "package is ${gameengine2_VERSION}, expected ${GE_EXPECT_VERSION}")
endif()

# One translation unit that includes every installed header by the path a
# game spells it, so a header missing from the package, or one that does not
# stand alone, fails here rather than in the game.
get_target_property(_ge_includes ge::core INTERFACE_INCLUDE_DIRECTORIES)
set(_ge_tu "${CMAKE_CURRENT_BINARY_DIR}/all_headers.cpp")
file(WRITE "${_ge_tu}" "// generated: every installed engine header\n")
foreach(_dir IN LISTS _ge_includes)
  file(GLOB_RECURSE _headers RELATIVE "${_dir}" "${_dir}/*.hpp" "${_dir}/*.h")
  foreach(_h IN LISTS _headers)
    file(APPEND "${_ge_tu}" "#include \"${_h}\"\n")
  endforeach()
endforeach()

add_executable(consumer main.cpp "${_ge_tu}")
target_link_libraries(consumer PRIVATE ge::core ge::racing ge::mountain)
target_compile_features(consumer PRIVATE cxx_std_20)

enable_testing()
add_test(NAME consumer COMMAND consumer)
