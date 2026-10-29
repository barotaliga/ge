// RAYS AGAINST A BAKE'S TRIANGLES: a bounding volume hierarchy built once over a scene's
// triangles (binned surface-area splits, four triangles a leaf at most), and the two questions a
// baker asks of it: the nearest triangle along a ray, and whether anything lies between two
// points. Single precision, built in one fixed order, so the same scene gives the same tree and
// the same hits on every machine.
//
// A hit's barycentrics (b1, b2) weigh the triangle's second and third corners. Rays start a
// little off a surface (the caller's bias); hits nearer than `t_min` are ignored.
//
// ge:summary The bake's ray tracer: a BVH over triangles, nearest hit and occlusion queries
// ge:keywords bake bvh ray trace raytrace intersect closest-hit occluded shadow-ray sah
// ge:keywords build_bvh closest_hit occluded moller-trumbore
// ge:skill render perf
// ge:test tests/bake_trace_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "bake/scene.hpp"

namespace ge::bake {

struct BvhNode {
  float lo[3], hi[3];
  uint32_t first;  // a leaf: its first triangle in Bvh::tris; an inner node: its left child
  uint32_t count;  // a leaf's triangles; 0 for an inner node (its right child is first + 1)
};

struct Bvh {
  std::vector<BvhNode> nodes;  // [0] the root
  std::vector<uint32_t> tris;  // triangle indices in leaf order
  // Each triangle's corner and two edges, in leaf order, for the intersection.
  std::vector<float> prim;  // 9 a triangle: v0, e1 = v1 − v0, e2 = v2 − v0
};

struct Ray {
  float o[3];
  float d[3];  // need not be unit; t is in its lengths
  float t_min = 1e-4f, t_max = 1e30f;
};

struct Hit {
  uint32_t tri = 0xffffffffu;  // kNoHit: none
  float t = 0, b1 = 0, b2 = 0;
  bool front = true;  // the ray met the side the triangle's winding faces
  bool hit() const { return tri != 0xffffffffu; }
};

Bvh build_bvh(const Scene& scene);
// The nearest triangle along `ray` within (t_min, t_max).
Hit closest_hit(const Bvh& bvh, const Ray& ray);
// Whether any triangle lies along `ray` within (t_min, t_max), either side (a shadow ray).
bool occluded(const Bvh& bvh, const Ray& ray);

}  // namespace ge::bake
