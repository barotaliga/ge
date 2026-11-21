// A map's place names as points (places.hpp): each place's middle, the nearest point, its name.
//
// ge:summary A map's place names: each place's middle, the nearest point and its name
// ge:keywords nearest_loc loc_name_at loc_places
// ge:skill shooter bots
// ge:test tests/classic_locs_test.cpp
#include "shooter/places.hpp"

namespace ge::shooter {

std::vector<Loc> loc_places(const Locs& locs) {
  std::vector<Loc> out;
  std::vector<int> count;
  for (const Loc& l : locs.points) {
    size_t i = 0;
    while (i < out.size() && out[i].name != l.name) i++;
    if (i == out.size()) {
      out.push_back({l.name, {0, 0, 0}, {}});
      count.push_back(0);
    }
    out[i].at = out[i].at + l.at;
    count[i]++;
  }
  for (size_t i = 0; i < out.size(); i++)
    out[i].at = out[i].at * (1.f / static_cast<float>(count[i]));
  return out;
}

const Loc* nearest_loc(const Locs& locs, Vec3f p, float range, const LocSight& seen) {
  const Loc* best = nullptr;
  float best_d = 0;
  for (const Loc& l : locs.points) {
    const Vec3f d = l.at - p;
    const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
    if (range > 0 && dd > range * range) continue;
    if (best && dd >= best_d) continue;
    if (seen && !seen(p, l.at)) continue;  // asked only of a point that would be nearer
    best = &l;
    best_d = dd;
  }
  return best;
}

std::string loc_name_at(const Locs& locs, Vec3f p, float range, const LocSight& seen) {
  const Loc* l = nearest_loc(locs, p, range, seen);
  return l ? l->name : std::string();
}

}  // namespace ge::shooter
