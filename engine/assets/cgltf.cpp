// cgltf's implementation, compiled once and alone: gltf.cpp includes only its declarations. The
// header is vendored as downloaded (cmake/Cgltf.cmake), so clang-tidy's findings in it are not
// this tree's to fix and are switched off here.
//
// ge:summary The cgltf library's implementation, in a translation unit of its own
// ge:keywords assets gltf cgltf vendored implementation
// ge:skill assets
// ge:test tests/assets_gltf_test.cpp
// NOLINTBEGIN
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
// NOLINTEND
