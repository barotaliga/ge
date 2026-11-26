// The BSP's frame lights chosen (light_pick.hpp).
//
// ge:summary The BSP's frame lights chosen: the drawn set, the shadowed few, the cube slots dealt
// ge:keywords bsp brush light-pick choose_lights deal_cubes light_reach box_leaves
// ge:skill brush render
// ge:test tests/brush_bsp_light_pick_test.cpp
#include "brush/app/light_pick.hpp"

#include <algorithm>
#include <cmath>

#include "brush/app/bsp_flames.hpp"
#include "render/view.hpp"

namespace ge::brush::app {

void light_sphere(const PointLight& l, bool bonfires, double out[4]) {
  out[0] = l.pos[0];
  out[1] = l.physical ? l.pos[1] : l.pos[1] + fire_light_lift(l, bonfires);
  out[2] = l.pos[2];
  out[3] = l.physical ? l.reach : l.reach * 1.5;
}

namespace {

// The engine's metres to the map's file units (z up).
void to_file(const double p[3], float out[3]) {
  out[0] = float(p[0] / kUnitM);
  out[1] = float(p[2] / kUnitM);
  out[2] = float(p[1] / kUnitM);
}

}  // namespace

LightReach light_reach(const Map& map, std::span<const PointLight> lights) {
  LightReach out;
  out.clusters.resize(lights.size());
  if (map.leaves.empty() || map.pvs.empty()) return out;  // no visibility: nothing to test
  std::vector<uint32_t> leaves(4096);
  for (size_t i = 0; i < lights.size(); ++i) {
    // The box round both spheres (a fire's light lifted as a torch's and as a bonfire's).
    double a[4], b[4];
    light_sphere(lights[i], false, a);
    light_sphere(lights[i], true, b);
    float lo[3], hi[3];
    {
      float ca[3], cb[3];
      to_file(a, ca);
      to_file(b, cb);
      const float r = float(std::max(a[3], b[3]) / kUnitM);
      for (int k = 0; k < 3; ++k) {
        lo[k] = std::min(ca[k], cb[k]) - r;
        hi[k] = std::max(ca[k], cb[k]) + r;
      }
    }
    const uint32_t n = box_leaves(map, lo, hi, leaves.data(), uint32_t(leaves.size()));
    std::vector<int32_t>& list = out.clusters[i];
    if (n > leaves.size()) {
      list.push_back(-1);  // too many to list: always seen
      continue;
    }
    for (uint32_t k = 0; k < n; ++k) {
      const Leaf& leaf = map.leaves[leaves[k]];
      if (leaf.solid) continue;
      list.push_back(leaf.cluster);
    }
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
    // A light whose reach is all in solid, or that reaches a leaf with no cluster: always seen
    // (nothing says it is not).
    if (list.empty() || list.front() < 0) list.assign(1, -1);
  }
  return out;
}

void choose_lights(const Map& map, std::span<const LightOffer> offers, const LightReach* reach,
                   const r::FrameView& view, const PickRules& rules, LightChoice& out) {
  (void)reach;
  out.drawn.clear();
  out.casters.clear();
  struct Pick {
    LightOffer o;
    double d;
  };
  std::vector<Pick> picks;
  for (const LightOffer& o : offers) {
    const PointLight& l = *o.light;
    const double r = l.physical ? l.reach : l.reach * 1.5;
    const double dx = l.pos[0] - view.eye[0], dy = l.pos[1] - view.eye[1],
                 dz = l.pos[2] - view.eye[2];
    const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist < r + 40) picks.push_back({o, dist - r});
  }
  std::sort(picks.begin(), picks.end(), [](const Pick& a, const Pick& b) { return a.d < b.d; });
  const int most = rules.overlay ? rules.overlay_most : std::min(rules.most, 512);
  if (picks.size() > size_t(most)) picks.resize(size_t(most));

  double ahead[3];
  r::look_dir(view.heading, view.pitch, ahead);
  const double half_y = std::tan(view.fov_y / 2);
  const double cone = std::atan(half_y * std::sqrt(1 + rules.widest * rules.widest));
  const auto in_view = [&](const double s[4]) {
    const double to[3] = {s[0] - view.eye[0], s[1] - view.eye[1], s[2] - view.eye[2]};
    const double dist = std::sqrt(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]);
    if (dist <= s[3]) return true;
    const double along = (to[0] * ahead[0] + to[1] * ahead[1] + to[2] * ahead[2]) / dist;
    const double off = std::acos(std::clamp(along, -1.0, 1.0));
    return off - std::asin(s[3] / dist) <= cone;
  };
  const auto cluster_of = [&](const double p[3]) {
    float f[3];
    to_file(p, f);
    return map.leaves.empty() ? -1 : map.leaves[leaf_at(map, f)].cluster;
  };
  const int32_t eye_cluster = cluster_of(view.eye);
  std::vector<size_t> aside, unseen;
  for (const Pick& p : picks) {
    const PointLight& l = *p.o.light;
    if (!l.physical && !p.o.dynamic) {
      double s[4];
      light_sphere(l, rules.bonfires, s);
      const int32_t at = cluster_of(l.pos);
      const bool seen = eye_cluster < 0 || at < 0 || cluster_sees(map, eye_cluster, at);
      (!seen ? unseen : in_view(s) ? out.casters : aside).push_back(out.drawn.size());
    }
    out.drawn.push_back(p.o);
  }
  out.casters.insert(out.casters.end(), aside.begin(), aside.end());
  out.casters.insert(out.casters.end(), unseen.begin(), unseen.end());
  if (int(out.casters.size()) > rules.shadowed) out.casters.resize(size_t(rules.shadowed));
}

void deal_cubes(CubeSlots& slots, std::span<const std::array<float, 4>> keys, size_t budget,
                double dt_s, double fade_s, CubeDeal& out) {
  (void)dt_s;
  (void)fade_s;
  ++slots.frame;
  out.slot.assign(keys.size(), -1);
  out.fade.assign(keys.size(), 1.0f);
  out.fresh.clear();
  out.waiting = 0;
  std::array<bool, kMaxCubeSlots> taken{};
  std::vector<size_t> homeless;
  for (size_t c = 0; c < keys.size(); ++c) {
    int slot = -1;
    for (int k = 0; k < slots.slots && slot < 0; ++k)
      if (slots.held[size_t(k)] && !taken[size_t(k)] && slots.key[size_t(k)] == keys[c]) slot = k;
    if (slot < 0) {
      homeless.push_back(c);
      continue;
    }
    taken[size_t(slot)] = true;
    slots.used[size_t(slot)] = slots.frame;
    out.slot[c] = slot;
  }
  for (const size_t c : homeless) {
    if (out.fresh.size() >= budget) {
      ++out.waiting;  // lit, unshadowed, this frame
      continue;
    }
    int slot = -1;
    for (int k = 0; k < slots.slots; ++k) {
      if (taken[size_t(k)]) continue;
      if (slot < 0 || slots.used[size_t(k)] < slots.used[size_t(slot)]) slot = k;
    }
    if (slot < 0) {
      ++out.waiting;
      continue;
    }
    taken[size_t(slot)] = true;
    slots.key[size_t(slot)] = keys[c];
    slots.held[size_t(slot)] = true;
    slots.used[size_t(slot)] = slots.frame;
    out.fresh.push_back(slot);
    out.slot[c] = slot;
  }
}

}  // namespace ge::brush::app
