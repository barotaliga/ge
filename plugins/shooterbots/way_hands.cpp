// The way through a campaign map (way_through.hpp), at the keys: the dive into water the plan
// closed, the reach into a step's box (ducked through a crawl), the shove off a wedge, the touch
// of a trigger beside him and the brake at a pit's lip.
//
// ge:summary The way through a map at the keys: dive, reach in, crawl, unwedge, touch, edge
// ge:keywords way_dive way_reach_in way_touch unwedge way_edge crawl reach-in closing-floor zombie
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/trace.hpp"
#include "shooter/usercmd.hpp"
#include "shooterbots/way_through.hpp"

namespace sb = ge::shooterbots;

namespace ge::shooterbots {

namespace {

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

bool touches(sh::Vec3f p, sh::Vec3f lo, sh::Vec3f hi) {
  return p.x + 16 >= lo.x && p.x - 16 <= hi.x && p.y + 16 >= lo.y && p.y - 16 <= hi.y &&
         p.z + 32 >= lo.z && p.z - 24 <= hi.z;
}

}  // namespace

void way_dive(const WayThrough& w, const MapNav& nav, const ge::shooterbots::BotView& s, int me,
              sh::MoveCmd& cmd) {
  if (w.out < 0) return;
  const ge::shooter::WorldEntity& self_e = s.world.entities[static_cast<size_t>(me)];
  const sh::Vec3f self = self_e.origin;
  const sb::NavExtraLink& in = nav.entries[static_cast<size_t>(w.dive)];
  const sb::NavExtraLink& out = nav.entries[static_cast<size_t>(w.out)];
  const auto at = [&](int32_t n) { return nav.graph.nodes[static_cast<size_t>(n)].origin; };
  sh::Vec3f to;
  if (self_e.waterlevel == 0) {
    if (dist(self, at(in.from)) > 160) return;  // the pilot takes him near the edge first
    to = at(in.to);
  } else {
    // Swimming: to the far side's water, then up onto its land.
    const sh::Vec3f wet = at(out.to);
    const float dx = wet.x - self.x, dy = wet.y - self.y;
    to = dx * dx + dy * dy > 48 * 48 ? wet : at(out.from);
  }
  const double dx = to.x - self.x, dy = to.y - self.y, dz = to.z - self.z;
  const double flat = std::sqrt(dx * dx + dy * dy);
  cmd.angles.y = static_cast<float>(ge::core::libm::atan2(dy, dx) * (180.0 / ge::core::kPi));
  cmd.angles.x = static_cast<float>(-ge::core::libm::atan2(dz, flat) * (180.0 / ge::core::kPi));
  cmd.forward = sh::quantize_axis(320);
  cmd.side = 0;
  if (self_e.waterlevel > 0 && dz > 8) cmd.buttons |= sh::kButtonJump;
}

void way_fall_steps(WayThrough& w, const MapNav& nav, const ge::shooterbots::BotView& s,
                    const ge::ai::Search& reach) {
  const sb::NavGraph& g = nav.graph;
  // One hung over the floor (e3m5's in the pit under the top floor, t30 for the second tube):
  // touched only falling through it, so reached from the floor beside the open air over it,
  // walked off and steered into it on the way down.
  for (WayStep& st : w.steps) {
    if ((st.kind != WayKind::trigger && st.kind != WayKind::secret) || st.reach_in ||
        (st.node >= 0 && touches(g.nodes[static_cast<size_t>(st.node)].origin, st.lo, st.hi)))
      continue;
    // Hung: no floor by it within a jump of its bottom (a trigger laid over a walk across a pit,
    // e1m2's, has the walk's nodes beside it; e3m5's in the pit has floors only far under and
    // far over it).
    const sh::Vec3f mid = (st.lo + st.hi) * 0.5f;
    bool hung = true;
    for (size_t a = 0; a < g.nodes.size() && hung; a++) {
      const sh::Vec3f o = g.nodes[a].origin;
      hung = !(o.x > st.lo.x - kWayTouchBy && o.x < st.hi.x + kWayTouchBy &&
               o.y > st.lo.y - kWayTouchBy && o.y < st.hi.y + kWayTouchBy &&
               o.z > st.lo.z - kWayHung && o.z < st.hi.z + kWayHung);
    }
    if (!hung) continue;
    // The open air over it: a fall from there passes through it.
    int32_t best = -1;
    float best_d = 0;
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sh::Vec3f o = g.nodes[a].origin;
      if (g.nodes[a].swim || o.z - 24 < st.hi.z || o.z - 24 > st.hi.z + kWayFallFrom ||
          o.x < st.lo.x - kWayFallBy || o.x > st.hi.x + kWayFallBy || o.y < st.lo.y - kWayFallBy ||
          o.y > st.hi.y + kWayFallBy)
        continue;
      const sh::Vec3f over{mid.x, mid.y, o.z};
      if (sh::trace_move(s.sv, over, {}, {}, {mid.x, mid.y, st.lo.z}, sh::kTraceNoMonsters, 1)
              .fraction < 1)
        continue;  // a floor over it: walked over, never fallen through
      // One he reaches first (the floor round the pit's top), then the nearest.
      const float d =
          dist(o, over) + (reach.reached(static_cast<int32_t>(a)) ? 0.0f : kWayFallFrom);
      if (best < 0 || d < best_d) {
        best = static_cast<int32_t>(a);
        best_d = d;
      }
    }
    const sh::Vec3f under{mid.x, mid.y,
                          best >= 0 ? g.nodes[static_cast<size_t>(best)].origin.z : 0};
    if (best < 0) continue;
    st.node = best;
    st.crawl = {g.nodes[static_cast<size_t>(best)].origin, under};
    st.reach_in = st.fall = true;
  }
}

void way_reach_in(WayThrough& w, const MapNav& nav, const ge::shooterbots::BotView& s, int me,
                  sh::MoveCmd& cmd) {
  const ge::shooter::WorldEntity& self_e = s.world.entities[static_cast<size_t>(me)];
  const sh::Vec3f self = self_e.origin;
  const double t = s.sv.time;
  const auto crawl_to = [&](sh::Vec3f to) {
    const double dx = to.x - self.x, dy = to.y - self.y;
    cmd.angles.y = static_cast<float>(ge::core::libm::atan2(dy, dx) * (180.0 / ge::core::kPi));
    cmd.angles.x = 0;
    cmd.forward = sh::quantize_axis(320);
    cmd.side = 0;
    cmd.up = sh::quantize_axis(-320);  // ducked
  };
  // The nearest of a crawl's points to him, and the next one on.
  const auto next_on = [&](const std::vector<sh::Vec3f>& way, size_t& k) {
    float best = 0;
    for (size_t i = 0; i < way.size(); i++) {
      // In three dimensions: the floor over a nook is not the nook.
      const float dx = way[i].x - self.x, dy = way[i].y - self.y, dz = way[i].z - self.z;
      const float d = dx * dx + dy * dy + dz * dz;
      if (i == 0 || d < best) {
        best = d;
        k = i;
      }
    }
    return best;
  };
  if (!w.crawl_out.empty()) {
    size_t k = 0;
    next_on(w.crawl_out, k);
    if (k + 1 >= w.crawl_out.size() || t - w.crawl_out_at > kWayCrawlOut_s) {
      w.crawl_out.clear();  // out (or given up on: the pilot's own way from here)
    } else {
      crawl_to(w.crawl_out[k + 1]);
      return;
    }
  }
  if (w.step < 0) return;
  const WayStep& st = w.steps[static_cast<size_t>(w.step)];
  if (!st.reach_in || st.done || st.node < 0) return;
  if (!st.crawl.empty()) {
    size_t k = 0;
    const float d = next_on(st.crawl, k);
    const bool by_node =
        dist(self, nav.graph.nodes[static_cast<size_t>(st.node)].origin) <= kWayTouchFrom;
    if (!by_node && d > kWayCrawlNear * kWayCrawlNear) return;
    if (k + 1 < st.crawl.size()) {
      crawl_to(st.crawl[k + 1]);
    } else {
      crawl_to((st.lo + st.hi) * 0.5f);
    }
    return;
  }
  if (dist(self, nav.graph.nodes[static_cast<size_t>(st.node)].origin) > kWayTouchFrom) return;
  // The box's nearest point, his box's middle held to it.
  const sh::Vec3f to{std::clamp(self.x, st.lo.x, st.hi.x), std::clamp(self.y, st.lo.y, st.hi.y),
                     std::clamp(self.z + 4, st.lo.z, st.hi.z) - 4};
  const double dx = to.x - self.x, dy = to.y - self.y, dz = to.z - self.z;
  const double flat = std::sqrt(dx * dx + dy * dy);
  cmd.angles.y = static_cast<float>(ge::core::libm::atan2(dy, dx) * (180.0 / ge::core::kPi));
  cmd.angles.x =
      self_e.waterlevel > 0
          ? static_cast<float>(-ge::core::libm::atan2(dz, flat) * (180.0 / ge::core::kPi))
          : 0.0f;
  cmd.forward = sh::quantize_axis(320);
  cmd.side = 0;
  if (self_e.waterlevel > 0 && dz > 8) cmd.buttons |= sh::kButtonJump;
}

namespace {

// A body that blocks as a box (a monster, a barrel; not a player) alive against him: its box within
// kWayAgainst of his.
constexpr float kWayAgainst = 8;
// The body's edict number, or -1: none.
int body_against(const ge::shooterbots::BotView& s, int me) {
  const sh::Edict& e = s.sv.edicts[static_cast<size_t>(me)];
  for (int k = s.sv.set.maxclients + 1; k < s.sv.num_edicts; k++) {
    const sh::Edict& o = s.sv.edicts[static_cast<size_t>(k)];
    if (o.free || o.health <= 0 || (o.solid != sh::kSolidBbox && o.solid != sh::kSolidSlidebox))
      continue;
    const sh::Vec3f lo = o.origin + o.mins, hi = o.origin + o.maxs;
    const sh::Vec3f mlo = e.origin + e.mins, mhi = e.origin + e.maxs;
    if (lo.x < mhi.x + kWayAgainst && hi.x > mlo.x - kWayAgainst && lo.y < mhi.y + kWayAgainst &&
        hi.y > mlo.y - kWayAgainst && lo.z < mhi.z + kWayAgainst && hi.z > mlo.z - kWayAgainst)
      return k;
  }
  return -1;
}

}  // namespace

void unwedge(Wedge& w, const ge::shooterbots::BotView& s, int me, double t, sh::MoveCmd& cmd) {
  const sh::Edict& e = s.sv.edicts[static_cast<size_t>(me)];
  if (e.health <= 0 || dist(e.origin, w.still_at) > kWayStuck) {
    w.still_at = e.origin;
    w.still_since = t;
    return;
  }
  if (t - w.still_since < kWayStuck_s || (cmd.forward == 0 && cmd.side == 0)) return;
  w.shoves++;
  const int phase = static_cast<int>(t - w.still_since);  // a side a second
  cmd.side = static_cast<int16_t>((phase & 1) ? 400 : -400);
  // Held up by a body (a monster, a barrel), he steps round it but never jumps onto it: a jump
  // there left him standing on a pack of soldiers' heads in base1, as no person would.
  const int body = body_against(s, me);
  if (body < 0 && static_cast<int>((t - w.still_since) * 4) % 4 == 0)
    cmd.buttons |= sh::kButtonJump;
  // Held up by a monster that is no fight of his (a zombie walking its beat in e1m3's corridor,
  // never his target): shot down out of the way, as a person would, while he steps round it.
  if (body >= 0 && (s.sv.edicts[static_cast<size_t>(body)].flags & sh::kFlagMonster) != 0) {
    const sh::Edict& b = s.sv.edicts[static_cast<size_t>(body)];
    const double dx = b.origin.x - e.origin.x, dy = b.origin.y - e.origin.y;
    const double dz = (b.origin.z + (b.mins.z + b.maxs.z) * 0.5) - (e.origin.z + 22);
    const double flat = std::sqrt(dx * dx + dy * dy);
    cmd.angles.y = static_cast<float>(ge::core::libm::atan2(dy, dx) * (180.0 / ge::core::kPi));
    cmd.angles.x = static_cast<float>(-ge::core::libm::atan2(dz, flat) * (180.0 / ge::core::kPi));
    cmd.buttons |= sh::kButtonAttack;
    w.shots++;
  }
}

bool way_touch(const WayThrough& w, const ge::shooterbots::BotView& s, int me, sh::MoveCmd& cmd) {
  if (w.step < 0) return false;
  const WayStep& st = w.steps[static_cast<size_t>(w.step)];
  if (w.out >= 0 || st.done ||
      !(st.kind == WayKind::exit || st.kind == WayKind::trigger || st.kind == WayKind::secret))
    return false;
  const sh::Edict& e = s.sv.edicts[static_cast<size_t>(me)];
  const sh::Vec3f self = e.origin;
  if (e.health <= 0 || touches(self, st.lo, st.hi)) return false;
  if (self.z - 24 > st.hi.z || self.z + 32 < st.lo.z) return false;  // above or under it
  const sh::Vec3f p{std::clamp(self.x, st.lo.x, st.hi.x), std::clamp(self.y, st.lo.y, st.hi.y),
                    self.z};
  const float dx = p.x - self.x, dy = p.y - self.y;
  const float d2 = dx * dx + dy * dy;
  if (d2 < 1 || d2 > kTouchNear * kTouchNear) return false;
  // Past kTouchReach, only straight at it over open floor: a wall between leaves it to the route.
  if (d2 > kTouchReach * kTouchReach &&
      sh::trace_move(s.sv, self, {}, {}, p, sh::kTraceNoMonsters, me).fraction < 1)
    return false;
  // Nearer, only where his box gets to touch it: base2's trigger behind a grate (the soldier on
  // the ledge over the sewer) held him pressed to the grate for good.
  const sh::HullTrace in =
      sh::trace_move(s.sv, self, sh::kPlayerMins, sh::kPlayerMaxs, p, sh::kTraceNoMonsters, me);
  if (in.fraction < 1 && !touches(in.endpos, st.lo, st.hi)) return false;
  float yaw = 0, pitch = 0;
  sh::aim_angles(self, p, yaw, pitch);
  cmd.angles = {0, yaw, 0};
  cmd.forward = 400;
  cmd.side = 0;
  return true;
}

bool way_edge(const MapNav& nav, const Fighter& f, const ge::shooterbots::BotView& s, int me,
              sh::MoveCmd& cmd) {
  const sh::Edict& e = s.sv.edicts[static_cast<size_t>(me)];
  if (e.health <= 0 || (e.flags & sh::kFlagOnGround) == 0 || e.waterlevel > 0 ||
      (cmd.forward == 0 && cmd.side == 0))
    return false;
  const sb::NavGraph& g = nav.graph;
  const auto trap = [&](int32_t n) {
    return n >= 0 && static_cast<size_t>(n) < nav.trap.size() && nav.trap[static_cast<size_t>(n)];
  };
  if (trap(sb::nav_nearest(g, e.origin))) return false;  // in it already: the way leads out
  // A route of his own that goes down there (an exit in a pit) is taken.
  const sb::Pilot& p = f.bot.pilot.pilot;
  for (size_t k = p.k; k < p.route.size() && k < p.k + 3; k++)
    if (trap(p.route[k])) return false;
  // The wish's heading under the view, and the floor a stride along it.
  const double yaw = cmd.angles.y * (ge::core::kPi / 180.0);
  const double c = ge::core::libm::cos(yaw), sn = ge::core::libm::sin(yaw);
  const double wx = c * cmd.forward + sn * cmd.side, wy = sn * cmd.forward - c * cmd.side;
  const double len = std::sqrt(wx * wx + wy * wy);
  if (len <= 0) return false;
  const sh::Vec3f dir{static_cast<float>(wx / len), static_cast<float>(wy / len), 0};
  const sh::Vec3f ahead = e.origin + dir * kEdgeLook;
  const sh::HullTrace side = sh::trace_move(s.sv, e.origin, sh::kPlayerMins, sh::kPlayerMaxs, ahead,
                                            sh::kTraceNoMonsters, me);
  if (side.fraction < 1) return false;  // a wall that way, not an edge
  const sh::HullTrace down =
      sh::trace_move(s.sv, ahead, sh::kPlayerMins, sh::kPlayerMaxs,
                     ahead - sh::Vec3f{0, 0, kEdgeDrop}, sh::kTraceNoMonsters, me);
  if (down.fraction < 1 && !trap(sb::nav_nearest(g, down.endpos))) return false;
  // No floor a step down: a drop. Onto a floor with a way back (base2's channel under the ledge
  // its soldier waits on) it is let be; only into the trap, or into no floor at all, braked.
  if (down.fraction >= 1) {
    const sh::HullTrace fall =
        sh::trace_move(s.sv, ahead, sh::kPlayerMins, sh::kPlayerMaxs,
                       ahead - sh::Vec3f{0, 0, kEdgeFall}, sh::kTraceNoMonsters, me);
    if (fall.fraction < 1 && !trap(sb::nav_nearest(g, fall.endpos))) return false;
  }
  // Over the lip of a pit he could not climb back out of: the keys let go, and a run into it
  // braked by the wish turned back along it.
  cmd.forward = static_cast<int16_t>(-cmd.forward);
  cmd.side = static_cast<int16_t>(-cmd.side);
  cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonJump);
  return true;
}

bool way_through_floor(sh::Vec3f a, sh::Vec3f b, sh::Vec3f lo, sh::Vec3f hi) {
  const float mid = (lo.z + hi.z) * 0.5f;
  if (!((a.z > mid && b.z <= mid) || (a.z < mid && b.z >= mid))) return false;
  const float t = (mid - a.z) / (b.z - a.z);
  const auto over = [&](float x, float y) {
    return x >= lo.x - 16 && x <= hi.x + 16 && y >= lo.y - 16 && y <= hi.y + 16;
  };
  const sh::Vec3f low = a.z < b.z ? a : b;
  return over(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t) || over(low.x, low.y);
}

// A trigger hung in a shaft is reached by a fall through it onto a node this far under it, units.
constexpr float kUnderShaft = 512;

int32_t way_node_in(const sb::NavGraph& g, sh::Vec3f lo, sh::Vec3f hi) {
  const sh::Vec3f mid = (lo + hi) * 0.5f;
  constexpr float kOnMover = 1e6f;
  int32_t best = -1;
  float best_d = 0;
  for (size_t n = 0; n < g.nodes.size(); n++) {
    const sh::Vec3f o = g.nodes[n].origin;
    if (!touches(o, lo, hi)) continue;
    // A node off any mover first: one on a door the trigger opens is shut until it is touched
    // (e1m2's exit trigger right before its doors took a node on them and was never reached).
    const float d = dist(o, mid) + (g.nodes[n].mover >= 0 ? kOnMover : 0.0f);
    if (best < 0 || d < best_d) {
      best = static_cast<int32_t>(n);
      best_d = d;
    }
  }
  if (best >= 0 && best_d < kOnMover) return best;
  // None inside (a thin trigger up in a doorway, one over a pit) or only on a mover (e2m6's trigger
  // round the pillar it lowers): the nearest in the box grown by a player's size, a step away
  // (`way_touch` walks him in); none there, no node at all (a step no route leads to).
  const sh::Vec3f grow{32, 32, 56};
  for (size_t n = 0; n < g.nodes.size(); n++) {
    const sh::Vec3f o = g.nodes[n].origin;
    if (!touches(o, lo - grow, hi + grow)) continue;
    // A node off any mover first: one on a door the trigger opens is shut until it is touched
    // (e1m2's exit trigger right before its doors took a node on them and was never reached).
    const float d = dist(o, mid) + (g.nodes[n].mover >= 0 ? kOnMover : 0.0f);
    if (best < 0 || d < best_d) {
      best = static_cast<int32_t>(n);
      best_d = d;
    }
  }
  if (best >= 0) return best;
  const int32_t n = sb::nav_nearest(g, mid);
  if (n >= 0 && dist(g.nodes[static_cast<size_t>(n)].origin, mid) < 256) return n;
  // Hung in a shaft (base2's first exit, a trigger halfway down a pit under a hatch): fallen
  // through, at the highest node standing under it.
  for (size_t k = 0; k < g.nodes.size(); k++) {
    const sh::Vec3f o = g.nodes[k].origin;
    if (o.x < lo.x || o.x > hi.x || o.y < lo.y || o.y > hi.y || o.z > lo.z ||
        o.z < lo.z - kUnderShaft)
      continue;
    if (best < 0 || o.z > g.nodes[static_cast<size_t>(best)].origin.z)
      best = static_cast<int32_t>(k);
  }
  if (best >= 0) return best;
  // The nearest node at all within reach (a trigger in a lift's shaft, passed riding it: e1m1's
  // under its hidden plat opens the secret wall at the top; the nav's nearest looks for a floor).
  for (size_t k = 0; k < g.nodes.size(); k++) {
    const float d = dist(g.nodes[k].origin, mid);
    if (d < 256 && (best < 0 || d < best_d)) {
      best = static_cast<int32_t>(k);
      best_d = d;
    }
  }
  return best;
}

void way_note_closer(WayThrough& w, const ge::shooterbots::BotView& s, int e, const char* sf) {
  if (e < 0 || !sf || (std::atoi(sf) & 1) == 0) return;
  const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
  const sh::Vec3f size = d.absmax - d.absmin;
  if (size.z < size.x && size.z < size.y) w.closers.push_back({e, d.origin, false, {}, {}});
}

bool way_closed_floors(WayThrough& w, const MapNav& nav, const ge::shooterbots::BotView& s) {
  const sb::NavGraph& g = nav.graph;
  w.closed_shut.clear();
  w.closed_from.clear();
  for (WayThrough::Closer& c : w.closers) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(c.door)];
    if (dist(d.origin, c.spawn) < 1) continue;
    if (!c.known && d.velocity.x == 0 && d.velocity.y == 0 && d.velocity.z == 0) {
      c.known = true;
      for (size_t a = 0; a < g.nodes.size(); a++) {
        const sb::NavNode& n = g.nodes[a];
        for (int32_t k = 0; k < n.links; k++) {
          const int32_t li = n.first_link + k;
          const sb::NavLink& l = g.links[static_cast<size_t>(li)];
          if (!way_through_floor(n.origin, g.nodes[static_cast<size_t>(l.to)].origin, d.absmin,
                                 d.absmax))
            continue;
          c.links.push_back(li);
          c.froms.push_back(static_cast<int32_t>(a));
        }
      }
    }
    w.closed_shut.insert(w.closed_shut.end(), c.links.begin(), c.links.end());
    w.closed_from.insert(w.closed_from.end(), c.froms.begin(), c.froms.end());
  }
  const bool changed = w.closed_shut != w.closed_was;
  w.closed_was = w.closed_shut;
  return changed;
}

void way_blast_steps(WayThrough& w, const Fighter& f, const MapNav& nav,
                     const ge::shooterbots::BotView& s) {
  const sb::NavGraph& g = nav.graph;
  if (w.complete && f.arsenal != nullptr && f.arsenal->blast_only != nullptr &&
      f.arsenal->blast_pickup != nullptr) {
    const auto model_of = [&](const sh::Edict& d) {
      return d.modelindex > 0 && static_cast<size_t>(d.modelindex) < s.models.size()
                 ? std::string_view(s.models[static_cast<size_t>(d.modelindex)])
                 : std::string_view();
    };
    bool undead = false;
    for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
      undead = undead || (!d.free && d.health > 0 && f.arsenal->blast_only(model_of(d)));
    }
    for (int e = s.sv.set.maxclients + 1; undead && e < s.sv.num_edicts; e++) {
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
      if (d.free || !f.arsenal->blast_pickup(model_of(d))) continue;
      WayStep st;
      st.kind = WayKind::trigger;
      st.lo = {d.origin.x - 16, d.origin.y - 16, d.origin.z};
      st.hi = {d.origin.x + 16, d.origin.y + 16, d.origin.z + 32};
      st.node = way_node_in(g, st.lo, st.hi);
      st.name = "blast weapon";
      st.first = true;
      w.steps.push_back(st);
    }
  }
}

}  // namespace ge::shooterbots
