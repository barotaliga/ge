// engine/bake/trace.hpp: the BVH's nearest hit is the brute-force nearest over every triangle,
// a hit's side and barycentrics are right, and a shadow ray is blocked exactly when something
// lies between.
//
// ge:summary Test: the bake's BVH against brute force, hit sides, barycentrics, occlusion
// ge:keywords test bake bvh ray trace closest_hit occluded brute-force
// ge:skill testing render
#include "bake/trace.hpp"

#include <cmath>

#include "bake/chart.hpp"
#include "check.hpp"
#include "core/prng.hpp"

namespace {

using namespace ge::bake;

Ray ray(float ox, float oy, float oz, float dx, float dy, float dz) {
  Ray r;
  r.o[0] = ox;
  r.o[1] = oy;
  r.o[2] = oz;
  r.d[0] = dx;
  r.d[1] = dy;
  r.d[2] = dz;
  return r;
}

}  // namespace

// A floor quad at y = 0 facing up: a ray down from above hits its front at t = height.
TEST(trace_floor_front_and_back) {
  Scene s;
  const double floor[4][3] = {{0, 0, 0}, {0, 0, 2}, {2, 0, 2}, {2, 0, 0}};
  add_quad(s, floor, 0);
  const Bvh bvh = build_bvh(s);
  Hit h = closest_hit(bvh, ray(0.5f, 3, 0.5f, 0, -1, 0));
  CHECK(h.hit());
  CHECK(std::fabs(h.t - 3) < 1e-5f);
  CHECK(h.front);
  h = closest_hit(bvh, ray(0.5f, -1, 0.5f, 0, 1, 0));
  CHECK(h.hit() && !h.front);
  CHECK(!closest_hit(bvh, ray(5, 3, 5, 0, -1, 0)).hit());
  // Barycentrics rebuild the hit point.
  h = closest_hit(bvh, ray(1.5f, 1, 0.25f, 0, -1, 0));
  const float* p = &s.positions[0];
  const uint32_t* i = &s.indices[h.tri * 3];
  float x = 0;
  for (int c = 0; c < 3; c++) {
    const float w = c == 0 ? 1 - h.b1 - h.b2 : c == 1 ? h.b1 : h.b2;
    x += p[i[c] * 3] * w;
  }
  CHECK(std::fabs(x - 1.5f) < 1e-5f);
}

// Shadow rays: blocked by a slab between, free when the slab is past the end.
TEST(trace_occluded) {
  Scene s;
  const double slab[4][3] = {{-1, 1, -1}, {-1, 1, 1}, {1, 1, 1}, {1, 1, -1}};
  add_quad(s, slab, 0);
  const Bvh bvh = build_bvh(s);
  Ray r = ray(0, 0, 0, 0, 1, 0);
  r.t_max = 2;
  CHECK(occluded(bvh, r));
  r.t_max = 0.9f;
  CHECK(!occluded(bvh, r));
}

// Five hundred random triangles: the BVH's nearest hit is brute force's for a thousand rays.
TEST(trace_matches_brute_force) {
  Scene s;
  ge::core::Rng rng = ge::core::make_rng(7);
  for (int t = 0; t < 500; t++) {
    const double cx = ge::core::range(rng, -10, 10), cy = ge::core::range(rng, -10, 10),
                 cz = ge::core::range(rng, -10, 10);
    for (int c = 0; c < 3; c++) {
      s.positions.push_back(static_cast<float>(cx + ge::core::range(rng, -1, 1)));
      s.positions.push_back(static_cast<float>(cy + ge::core::range(rng, -1, 1)));
      s.positions.push_back(static_cast<float>(cz + ge::core::range(rng, -1, 1)));
      s.indices.push_back(static_cast<uint32_t>(t * 3 + c));
    }
    s.tri_material.push_back(0);
    s.tri_chart.push_back(kNoChart);
  }
  const Bvh bvh = build_bvh(s);
  Scene one = s;  // one triangle at a time, for the brute force
  int hits = 0;
  for (int k = 0; k < 1000; k++) {
    const Ray r = ray(static_cast<float>(ge::core::range(rng, -12, 12)),
                      static_cast<float>(ge::core::range(rng, -12, 12)),
                      static_cast<float>(ge::core::range(rng, -12, 12)),
                      static_cast<float>(ge::core::range(rng, -1, 1)),
                      static_cast<float>(ge::core::range(rng, -1, 1)),
                      static_cast<float>(ge::core::range(rng, -1, 1)));
    float best = 1e30f;
    uint32_t tri = 0xffffffffu;
    for (uint32_t t = 0; t < 500; t++) {
      one.indices.assign(s.indices.begin() + t * 3, s.indices.begin() + t * 3 + 3);
      const Bvh b1 = build_bvh(one);
      const Hit h = closest_hit(b1, r);
      if (h.hit() && h.t < best) {
        best = h.t;
        tri = t;
      }
    }
    const Hit h = closest_hit(bvh, r);
    CHECK(h.tri == tri);
    if (h.hit()) {
      hits++;
      CHECK(h.t == best);
      Ray sr = r;
      sr.t_max = best * 1.001f;
      CHECK(occluded(bvh, sr));
      sr.t_max = best * 0.999f;
      CHECK(!occluded(bvh, sr));
    }
  }
  CHECK(hits > 50);
}
