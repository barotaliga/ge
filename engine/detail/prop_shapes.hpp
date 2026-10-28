// THE PROPS' SHAPES, built in code: a lump of rubble (an icosahedron cut once, its corners pushed
// in and out by a hash and squashed), a piece of debris (an even variant a flat shard of broken
// slab, an odd one a splinter of timber), and a cushion of moss (a lumpy dome). Faceted (every
// face its own vertices and normal, so a low-poly shape reads as cut), a unit across, its base
// at y = 0 and up along +y; a prop's `size_m` scales it and its `up` and `yaw_rad` turn it.
//
// ge:summary The detail props' faceted shapes: rubble lumps, shards and splinters, moss cushions
// ge:keywords detail props prop-mesh shape rubble rock icosahedron debris shard splinter moss
// ge:keywords cushion faceted prop_mesh prop_transform
// ge:skill detail
// ge:test tests/detail_props_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "detail/props.hpp"

namespace ge::detail {

struct PropMesh {
  std::vector<Vec3> pos, nrm;  // three a triangle, faceted
};

PropMesh prop_mesh(PropKind kind, uint32_t variant);

// The prop's frame: its right, up and forward axes (scaled by its size) and its base.
struct PropFrame {
  Vec3 x, y, z, origin;
};
PropFrame prop_frame(const Prop& p);
inline Vec3 to_world(const PropFrame& f, const Vec3& v) {
  return add(add(add(f.origin, mul(f.x, v.x)), mul(f.y, v.y)), mul(f.z, v.z));
}

}  // namespace ge::detail
