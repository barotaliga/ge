// The rooms labs' grass drawn on the CPU: a cover's blades at rest as thin strips a lab's model
// view draws (lab::Shape), the shapes the CPU's `blade_shape` gives, which the GPU draws too.
//
// ge:summary Rooms labs: a grass cover's blades as strips in a lab model-view shape
// ge:keywords lab grass blades strips shape model-view draw rooms
// ge:skill rooms grass labs
#pragma once

#include <cmath>
#include <cstdint>

#include "grass/cover.hpp"
#include "grass/sway.hpp"
#include "lab/model_view.hpp"

namespace ge::rooms::labs {

// Every blade of `cover` as a strip of three quads along its calm shape, both windings (a blade
// is seen from either side), in material `part`, narrowing to its tip.
inline void add_blades(lab::Shape& s, const ge::grass::Cover& cover, uint8_t part) {
  const float calm[2] = {0, 0};
  for (const ge::grass::Blade& b : ge::grass::scatter_all(cover)) {
    const ge::grass::Shape sh =
        ge::grass::blade_shape(cover.kinds[ge::grass::kind_of(b)], b, calm, 0);
    // The blade's flat faces across its lean: the root's side vector, level.
    const float lx = sh.v2[0] - sh.v0[0], lz = sh.v2[2] - sh.v0[2];
    const float ll = std::sqrt(lx * lx + lz * lz);
    const float side[2] = {ll > 1e-4f ? -lz / ll : 1.0f, ll > 1e-4f ? lx / ll : 0.0f};
    const auto base = uint32_t(s.part.size());
    for (int i = 0; i <= 3; ++i) {
      const float u = float(i) / 3;
      float p[3];
      ge::grass::bezier_point(sh, u, p);
      const float half = sh.width * 0.5f * (1 - u);
      for (const float e : {-1.0f, 1.0f}) {
        s.pos.insert(s.pos.end(), {p[0] + e * half * side[0], p[1], p[2] + e * half * side[1]});
        s.nrm.insert(s.nrm.end(), {0, 1, 0});
        s.part.push_back(part);
        s.top.push_back(0);
      }
    }
    for (uint32_t i = 0; i < 3; ++i) {
      const uint32_t a = base + i * 2, q[4] = {a, a + 1, a + 3, a + 2};
      s.idx.insert(s.idx.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
      s.idx.insert(s.idx.end(), {q[0], q[2], q[1], q[0], q[3], q[2]});
    }
  }
}

}  // namespace ge::rooms::labs
