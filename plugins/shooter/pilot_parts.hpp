// THE PILOT'S SMALL PARTS (pilot.hpp): the plan's geometry (distance across, headings, angles
// turned by whole turns) and the link between two nodes, shared by pilot.cpp and its split-out
// parts (pilot_movers.cpp, pilot_gaps.cpp). Only those include this.
//
// ge:summary The pilot's small parts: distance across, headings, angles wrapped, a node's link
// ge:keywords pilot across heading yaw_at unwrap wrap link_of pilot-parts
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#pragma once

#include <cstdint>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/nav.hpp"
#include "shooter/pilot_movers.hpp"
#include "shooter/vec.hpp"

namespace ge::shooter::pilot_parts {

inline float across(Vec3f a, Vec3f b) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  return root_of(dx * dx + dy * dy);
}

inline float heading(float dx, float dy) {
  const double r = core::libm::atan2(static_cast<double>(dy), static_cast<double>(dx));
  return static_cast<float>(r * (180.0 / core::kPi));
}

inline float yaw_at(Vec3f from, Vec3f to) { return heading(to.x - from.x, to.y - from.y); }

// `deg` moved by whole turns to within half a turn of `near`.
inline float unwrap(float deg, float near) {
  float d = deg - near;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return near + d;
}

inline float wrap(float deg) { return unwrap(deg, 0); }

inline const NavLink* link_of(const NavGraph& g, int32_t a, int32_t b) {
  const NavNode& n = node(g, a);
  for (int32_t k = 0; k < n.links; k++) {
    const NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
    if (l.to == b) return &l;
  }
  return nullptr;
}

}  // namespace ge::shooter::pilot_parts
