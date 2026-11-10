// ge:summary The gap tricks proven into the nav as jump links at a hopper's speed
// ge:keywords gap-tricks nav_add_gap_tricks speed-jump prove_link
// ge:skill shooter bots
// ge:test tests/shooter_nav_gaps_test.cpp
#include "shooter/nav_gaps.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/framemove.hpp"

namespace ge::shooter {

namespace {

float across_of(Vec3f a, Vec3f b) {
  const float dx = a.x - b.x, dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

struct Near {
  float d;
  int32_t n;
};

}  // namespace

JumpFlight fly_jump(const MoveWorld& world, const MoveSettings& move, Vec3f origin, Vec3f velocity,
                    Vec3f aim, int msec, float limit_s) {
  JumpFlight out;
  MoveState s;
  s.origin = origin;
  s.velocity = velocity;
  s.maxspeed = move.maxspeed;
  const float step_s = static_cast<float>(msec) * 0.001f;
  float yaw = 0;
  for (int i = 0; static_cast<float>(i) * step_s < limit_s; i++) {
    if (across_of(s.origin, aim) > 1)
      yaw = static_cast<float>(core::libm::atan2(aim.y - s.origin.y, aim.x - s.origin.x) *
                               (180.0 / core::kPi));
    MoveCmd c;
    c.msec = static_cast<uint8_t>(msec);
    c.angles = {0, yaw, 0};
    c.forward = static_cast<int16_t>(move.maxspeed);
    c.buttons = i == 0 ? kButtonJump : 0;
    player_move(s, c, move, world);
    out.end = s.origin;
    out.time_s = static_cast<float>(i + 1) * step_s;
    if (i > 0 && s.ground >= 0) {
      out.landed = true;
      return out;
    }
  }
  return out;
}

NavGapCounts nav_add_gap_tricks(const MoveWorld& world, const MoveSettings& move, NavGraph& g,
                                std::span<const NavGapTrick> tricks, std::span<const uint8_t> back,
                                const NavGapSettings& set) {
  NavGapCounts out;
  out.from.assign(tricks.size(), -1);
  if (!back.empty() && back.size() != g.nodes.size()) return out;
  const auto usable = [&](size_t n) {
    const NavNode& nd = g.nodes[n];
    return nd.mover < 0 && !nd.wet && !nd.swim && nd.ride < 0 && (back.empty() || back[n] != 0);
  };
  // Where a proof came down, as a node: the floor node it stands on (within 24 across and a
  // step up or down), within `land` across and 32 up or down of people's landing; -1 none.
  const auto landed = [&](const NavProof& pr, Vec3f people) {
    const int32_t q = nav_nearest(g, pr.end);
    if (q < 0 || !usable(static_cast<size_t>(q))) return -1;
    const Vec3f o = g.nodes[static_cast<size_t>(q)].origin;
    const float dz = pr.end.z - o.z, pz = o.z - people.z;
    const bool on = across_of(pr.end, o) <= 24 && dz <= kStepHeight && dz >= -kStepHeight;
    const bool there = across_of(o, people) <= set.land && pz <= 32 && pz >= -32;
    return on && there ? q : -1;
  };
  std::vector<NavExtraLink> extra;
  for (size_t t = 0; t < tricks.size(); t++) {
    const NavGapTrick& tr = tricks[t];
    std::vector<Near> starts, ends;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      if (!usable(n)) continue;
      const Vec3f o = g.nodes[n].origin;
      const Vec3f d = o - tr.from;
      const float dist = std::sqrt(dot(d, d));
      if (dist <= set.near) starts.push_back({dist, static_cast<int32_t>(n)});
      const float a = across_of(o, tr.to), dz = o.z - tr.to.z;
      if (a <= set.land && dz <= 32 && dz >= -32) ends.push_back({a, static_cast<int32_t>(n)});
    }
    const auto order = [](const Near& a, const Near& b) {
      return a.d != b.d ? a.d < b.d : a.n < b.n;
    };
    std::sort(starts.begin(), starts.end(), order);
    std::sort(ends.begin(), ends.end(), order);
    if (static_cast<int>(starts.size()) > set.takeoffs)
      starts.resize(static_cast<size_t>(set.takeoffs));
    if (static_cast<int>(ends.size()) > set.landings)
      ends.resize(static_cast<size_t>(set.landings));
    const float speeds[2] = {tr.speed, tr.speed * set.faster};
    bool done = false, run_made = false;
    for (size_t x = 0; x < starts.size() && !done; x++)
      for (size_t y = 0; y < ends.size() && !done; y++) {
        const int32_t a = starts[x].n, b = ends[y].n;
        if (a == b) continue;
        const Vec3f from = g.nodes[static_cast<size_t>(a)].origin;
        const Vec3f to = g.nodes[static_cast<size_t>(b)].origin;
        out.proofs++;
        if (landed(prove_link(world, move, from, to, NavKind::jump, move.maxspeed, g.set), tr.to) >=
            0) {
          run_made = true;  // a run's jump: no speed jump
          continue;
        }
        for (const float v : speeds) {
          out.proofs++;
          const NavProof pr = prove_link(world, move, from, to, NavKind::jump, v, g.set);
          const int32_t q = landed(pr, tr.to);
          if (q < 0 || q == a) continue;
          NavExtraLink l{a, q, NavKind::jump, pr.time_s + set.cost_s, v};
          l.level = tr.level;
          l.gap = true;
          extra.push_back(l);
          out.held++;
          out.from[t] = a;
          done = true;
          break;
        }
      }
    if (!done && run_made) out.run_makes++;
  }
  nav_add_links(g, extra);
  return out;
}

}  // namespace ge::shooter
