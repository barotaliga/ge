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
#include "core/libm/libm.hpp"
#include "render/view.hpp"

namespace ge::brush::app {

namespace r = ge::render;
namespace libm = ge::core::libm;

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

namespace {

// Whether any cluster of `list` is set in `seen` (one bit a cluster); -1 or an empty `seen`: yes.
bool any_seen(std::span<const int32_t> list, const std::vector<uint8_t>& seen) {
  if (seen.empty()) return true;
  for (const int32_t c : list) {
    if (c < 0) return true;
    if (size_t(c >> 3) < seen.size() && (seen[size_t(c) >> 3] >> (c & 7) & 1) != 0) return true;
  }
  return false;
}

}  // namespace

void choose_lights(const Map& map, std::span<const LightOffer> offers, const LightReach* reach,
                   const r::FrameView& view, const PickRules& rules, LightChoice& out) {
  out.drawn.clear();
  out.casters.clear();
  struct Pick {
    LightOffer o;
    double d;     // the distance less the reach, m: the order
    uint32_t at;  // the offer's index: ties broken by it
  };
  std::vector<Pick> picks;
  picks.reserve(offers.size());
  if (rules.overlay) {
    // The lightmaps' figures' shadows: the few lights nearest the eye, for their cubes alone.
    for (size_t i = 0; i < offers.size(); ++i) {
      double s[4];
      light_sphere(*offers[i].light, rules.bonfires, s);
      const double dx = s[0] - view.eye[0], dy = s[1] - view.eye[1], dz = s[2] - view.eye[2];
      const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
      if (dist < s[3] + 40) picks.push_back({offers[i], dist - s[3], uint32_t(i)});
    }
  } else {
    // The eye's potentially visible set, a hand's breadth round it so an eye on a leaf's edge
    // sees from both sides.
    std::vector<uint8_t> seen;
    if (!map.leaves.empty() && !map.pvs.empty()) {
      float fe[3];
      to_file(view.eye, fe);
      clusters_seen_from(map, fe, 4.0f, seen);
    }
    double ahead[3];
    r::look_dir(view.heading, view.pitch, ahead);
    const double half_y = libm::tan(view.fov_y / 2);
    const double cone = libm::atan(half_y * std::sqrt(1 + rules.widest * rules.widest));
    std::vector<uint32_t> leaves(512);
    std::vector<int32_t> clusters;
    for (size_t i = 0; i < offers.size(); ++i) {
      const PointLight& l = *offers[i].light;
      double s[4];
      light_sphere(l, rules.bonfires, s);
      const double to[3] = {s[0] - view.eye[0], s[1] - view.eye[1], s[2] - view.eye[2]};
      const double dist = std::sqrt(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]);
      // Its reach inside the cone round the look that holds the widest screen.
      if (dist > s[3]) {
        const double along = (to[0] * ahead[0] + to[1] * ahead[1] + to[2] * ahead[2]) / dist;
        const double off = libm::acos(std::clamp(along, -1.0, 1.0));
        if (off - libm::asin(s[3] / dist) > cone) continue;
      }
      // And in a cluster the eye's set holds: the map's lights by their lists made once, the
      // rest (the pools', the teleporters', the moment's) by the leaves their reach's box meets.
      if (!seen.empty()) {
        if (reach && i < reach->clusters.size()) {
          if (!reach->clusters[i].empty() && !any_seen(reach->clusters[i], seen)) continue;
        } else {
          float c[3];
          to_file(s, c);
          const float rr = float(s[3] / kUnitM);
          const float lo[3] = {c[0] - rr, c[1] - rr, c[2] - rr};
          const float hi[3] = {c[0] + rr, c[1] + rr, c[2] + rr};
          const uint32_t n = box_leaves(map, lo, hi, leaves.data(), uint32_t(leaves.size()));
          if (n <= leaves.size()) {
            clusters.clear();
            for (uint32_t k = 0; k < n; ++k) {
              const Leaf& leaf = map.leaves[leaves[k]];
              if (!leaf.solid) clusters.push_back(leaf.cluster);
            }
            if (!clusters.empty() && !any_seen(clusters, seen)) continue;
          }
        }
      }
      picks.push_back({offers[i], dist - s[3], uint32_t(i)});
    }
  }
  std::sort(picks.begin(), picks.end(),
            [](const Pick& a, const Pick& b) { return a.d < b.d || (a.d == b.d && a.at < b.at); });
  const int most = rules.overlay ? rules.overlay_most : std::clamp(rules.most, 0, kMaxFrameLights);
  if (picks.size() > size_t(most)) picks.resize(size_t(most));
  for (const Pick& p : picks) {
    // The map's own lights cast shadows (a pool's glow and a teleporter's are spread over their
    // surface; a dynamic light moves or goes out, so its cube would be drawn every frame).
    if (rules.shadowed > 0 && !p.o.light->physical && !p.o.dynamic)
      out.casters.push_back(out.drawn.size());
    out.drawn.push_back(p.o);
  }
}

void deal_cubes(CubeSlots& slots, std::span<const std::array<float, 4>> keys, size_t budget,
                double dt_s, double fade_s, CubeDeal& out) {
  ++slots.frame;
  const int count = std::clamp(slots.slots, 0, kMaxCubeSlots);
  out.slot.assign(keys.size(), -1);
  out.fade.assign(keys.size(), 0.0f);
  out.fresh.clear();
  out.waiting = 0;
  // All at once (a map's first frame that shadows: the load's): every cube whole at once.
  const bool at_once = budget >= size_t(count);
  const float step = fade_s > 0 && !at_once ? float(std::max(dt_s, 0.0) / fade_s) : 1.0f;
  std::array<bool, kMaxCubeSlots> taken{};
  std::array<size_t, kMaxCubeSlots> rank{};  // a taken slot's light's place in `keys`
  std::vector<size_t> homeless;
  // The lights that hold their cube: the first `count` (the most important) fade in; one past
  // them that another needs the slot of is fading out, and lets its slot go once dark.
  for (size_t c = 0; c < keys.size(); ++c) {
    int slot = -1;
    for (int k = 0; k < count && slot < 0; ++k)
      if (slots.held[size_t(k)] && !taken[size_t(k)] && slots.key[size_t(k)] == keys[c]) slot = k;
    if (slot < 0) {
      homeless.push_back(c);
      continue;
    }
    const size_t k = size_t(slot);
    slots.used[k] = slots.frame;
    if (c < size_t(count)) slots.leaving[k] = false;
    slots.fade[k] = slots.leaving[k] ? slots.fade[k] - step : std::min(1.0f, slots.fade[k] + step);
    if (slots.fade[k] <= 0) {
      slots.held[k] = false;  // faded out: free for another this frame
      slots.leaving[k] = false;
      slots.fade[k] = 0;
      ++out.waiting;
      continue;
    }
    taken[k] = true;
    rank[k] = c;
    out.slot[c] = slot;
    out.fade[c] = slots.fade[k];
  }
  // The rest, most important first, into the slots no light of this frame holds, the one unused
  // longest first, at most `budget` a frame; a light new to the first `count` with no slot free
  // has the least important light past them fade out of its slot. Until its cube is in, a light
  // waits dark: never unshadowed through the walls.
  for (const size_t c : homeless) {
    int slot = -1;
    if (out.fresh.size() < budget) {
      for (int k = 0; k < count; ++k) {
        if (taken[size_t(k)]) continue;
        if (slot < 0 || slots.used[size_t(k)] < slots.used[size_t(slot)]) slot = k;
      }
    }
    if (slot < 0) {
      ++out.waiting;
      if (c < size_t(count)) {
        int victim = -1;
        for (int k = 0; k < count; ++k) {
          if (!taken[size_t(k)] || slots.leaving[size_t(k)] || rank[size_t(k)] < size_t(count))
            continue;
          if (victim < 0 || rank[size_t(k)] > rank[size_t(victim)]) victim = k;
        }
        if (victim >= 0) slots.leaving[size_t(victim)] = true;
      }
      continue;
    }
    const size_t k = size_t(slot);
    taken[k] = true;
    rank[k] = c;
    slots.key[k] = keys[c];
    slots.held[k] = true;
    slots.leaving[k] = false;
    slots.used[k] = slots.frame;
    // Its cube is drawn this frame: it starts fading in now.
    slots.fade[k] = std::min(1.0f, step);
    out.fresh.push_back(slot);
    out.slot[c] = slot;
    out.fade[c] = slots.fade[k];
  }
}

}  // namespace ge::brush::app
