// THE PLAN (pilot.hpp `pilot_go`, `pilot_chain`, `pilot_retarget`, `pilot_unchain`): the way
// round the doors barred and shut and the links cut, a gap too near for the run left off it, a
// chain's pickup planned through, a goal retargeted on through the node ahead; and the link's
// state restarted, the route advanced and planned again (pilot_step_parts.hpp), a flight out of a
// pit landed out.
//
// ge:summary The pilot's plan: the way planned, chained, retargeted, restarted and advanced
// ge:keywords pilot pilot_go pilot_chain pilot_retarget pilot_unchain plan_way replan restart
// ge:keywords advance gap_too_near gapnear landed_out retarget chain via
// ge:skill shooterbots bots
// ge:test tests/shooterbots_pilot_test.cpp
#include <algorithm>
#include <utility>
#include <vector>

#include "shooter/world.hpp"
#include "shooterbots/escape.hpp"
#include "shooterbots/pilot.hpp"
#include "shooterbots/pilot_gaps.hpp"
#include "shooterbots/pilot_step_parts.hpp"

#include "shooterbots/kit.hpp"

namespace ge::shooterbots {

namespace pilot_parts {

namespace {

constexpr float kEscapeLanded = 128;  // a landing out counts this near the escape's ledge, units

}  // namespace

// Out of a pit (escape.hpp): standing out of the liquid near the escape's ledge (not respawned
// elsewhere after dying in its flight).
bool landed_out(const Pilot& p, const PilotSense& s) {
  if (p.escape_on < 0 || static_cast<size_t>(p.escape_on) >= p.g->escapes.size()) return false;
  const NavEscape& e = p.g->escapes[static_cast<size_t>(p.escape_on)];
  if (e.to < 0) return false;
  const Vec3f ledge = node(*p.g, e.to).origin;
  const bool dry = (s.world != nullptr ? water_level(*s.world, s.origin) : s.waterlevel) == 0;
  return dry && across(s.origin, ledge) <= kEscapeLanded && s.origin.z > ledge.z - kEscapeLanded &&
         s.origin.z < ledge.z + kEscapeLanded;
}

void restart(Pilot& p, const PilotSense& s) {
  p.recipe = -1;
  p.best = 1e30f;
  p.best_s = static_cast<float>(p.stats.time_s);
  p.wait_s = 0;
  p.airborne = false;
  p.grenade_s = -1;
  (void)s;
}

void advance(Pilot& p, const PilotSense& s) {
  p.k++;
  restart(p, s);
  if (p.mode == PilotMode::wait || p.mode == PilotMode::ride || p.mode == PilotMode::air ||
      p.mode == PilotMode::jump || p.mode == PilotMode::run_up || p.mode == PilotMode::swim ||
      p.mode == PilotMode::grenade)
    p.mode = PilotMode::walk;
}

bool replan(Pilot& p, const PilotSense& s) {
  const int32_t goal = p.goal;
  const PilotStats keep = p.stats;
  // A chain's pickup still ahead is planned through again.
  const int32_t via = p.via_k >= 0 ? p.route[static_cast<size_t>(p.via_k)] : -1;
  const Vec3f via_at = p.via_at;
  bool ok = false;
  if (via >= 0 && pilot_go(p, via, s)) {
    ok = true;
    if (!pilot_chain(p, goal, via_at)) {  // no way on from it: the goal is the pickup
      p.goal = goal;
      ok = pilot_go(p, goal, s);
    }
  } else {
    ok = pilot_go(p, goal, s);
  }
  p.stats.plans = keep.plans + 1;
  p.stats.vias = keep.vias;
  return ok;
}

}  // namespace pilot_parts

namespace {

using namespace pilot_parts;

// A goal retargeted (`pilot_retarget`) runs on through the node ahead when that way is no more
// than this slower than a fresh plan from where the player stands, s.
constexpr float kRetargetSlack_s = 0.25f;

// (`fieldway`) Whether every link of `nodes` is there and open now: no mover barred on it or in
// its end node, not cut (`nav_route`'s rule) nor shut, and taken by `j` (a gap trick: built to).
bool way_open(const NavGraph& g, std::span<const int32_t> nodes, std::span<const int32_t> barred,
              std::span<const NavExtraLink> cut, std::span<const NavExtraLink> shut,
              const NavJumper& j) {
  const auto bars = [&](int32_t m) {
    if (m < 0) return false;
    for (const int32_t b : barred)
      if (b == m) return true;
    return false;
  };
  const auto in = [](std::span<const NavExtraLink> list, int32_t a, int32_t b) {
    for (const NavExtraLink& c : list)
      if (c.from == a && c.to == b) return true;
    return false;
  };
  for (size_t i = 0; i + 1 < nodes.size(); i++) {
    const NavLink* l = link_of(g, nodes[i], nodes[i + 1]);
    if (l == nullptr || bars(l->mover) || bars(g.nodes[static_cast<size_t>(l->to)].mover))
      return false;
    if ((l->mover >= 0 || l->gap || j.cuts_all) && in(cut, nodes[i], nodes[i + 1])) return false;
    if (in(shut, nodes[i], nodes[i + 1])) return false;
    if (!j.takes(*l) && !(l->gap && j.gap_build_s >= 0 && l->level <= j.level)) return false;
  }
  return true;
}

// The nodes from `from` to `goal` round the doors barred and shut and the links cut, missed and
// given up; empty when there is no way.
NavRoute plan_way(Pilot& p, int32_t from, int32_t goal) {
  int32_t barred[4 + 60];
  int nb = 0;
  for (const int32_t b : p.barred)
    if (b >= 0) barred[nb++] = b;
  for (const int32_t b : p.shut)
    if (b >= 0 && nb < 64) barred[nb++] = b;
  std::span<const NavExtraLink> cut = p.cut;
  bool gaps_off = false;
  for (const double until : p.gaps_off_s) gaps_off = gaps_off || until > p.clock_s;
  // (`bridgeseen`) The links onto a bridge still in shut on every link (they carry no mover).
  const bool shut_every = p.set.shut_every && !p.shut_links.empty();
  if ((!p.shut_links.empty() && !shut_every) || gaps_off || p.near_cut.from >= 0) {
    p.cut_all = p.cut;
    if (!shut_every) p.cut_all.insert(p.cut_all.end(), p.shut_links.begin(), p.shut_links.end());
    for (size_t i = 0; i < p.gaps_off.size(); i++)
      if (p.gaps_off_s[i] > p.clock_s) p.cut_all.push_back(p.gaps_off[i]);
    if (p.near_cut.from >= 0) p.cut_all.push_back(p.near_cut);
    cut = p.cut_all;
  }
  const std::span<const int32_t> bars(barred, static_cast<size_t>(nb));
  const std::span<const NavExtraLink> shut =
      shut_every ? std::span<const NavExtraLink>(p.shut_links) : std::span<const NavExtraLink>{};
  // (`fieldway`) The goal's field's way, when nothing on it is barred, cut or shut now.
  if (p.set.fields != nullptr && from >= 0 && goal >= 0) {
    const NavField& f = nav_field(*p.set.fields, goal, p.set.jumper);
    NavFieldWay w;
    if (nav_field_way(*p.set.fields, f, from, w) && w.errand < 0 &&
        way_open(*p.g, w.nodes, bars, cut, shut, p.set.jumper)) {
      p.stats.field_ways++;
      NavRoute r;
      r.nodes = std::move(w.nodes);
      r.time_s = w.time_s;
      return r;
    }
  }
  return nav_route(*p.g, from, goal, p.search, 1, bars, cut, p.set.jumper, shut);
}

// (`gapnear`) The route's first gap trick when its takeoff is within `kGapCurveFrom` of the player
// and his run does not already head at it at `kGapNearShare` of its speed: a run that cannot take
// it (it would be given up on the next landing and kept off every route for 10 s); else none.
NavExtraLink gap_too_near(const Pilot& p, const PilotSense& s, std::span<const int32_t> route) {
  for (size_t j = 0; j + 1 < route.size(); j++) {
    const NavLink* l = link_of(*p.g, route[j], route[j + 1]);
    if (l == nullptr || !l->gap) continue;
    const Vec3f S = node(*p.g, route[j]).origin, T = node(*p.g, route[j + 1]).origin;
    if (across(s.origin, S) > kGapCurveFrom) break;
    Vec3f dir{T.x - S.x, T.y - S.y, 0};
    normalize(dir);
    if (dot(Vec3f{s.velocity.x, s.velocity.y, 0}, dir) >= l->speed * kGapNearShare) break;
    return {route[j], route[j + 1], NavKind::jump, l->time_s};
  }
  return {-1, -1, NavKind::jump, 0, 0};
}

}  // namespace

bool pilot_go(Pilot& p, int32_t goal, const PilotSense& s) {
  // A rocket out of a pit in flight is flown to its ledge first: the new goal planned on landing.
  // (Dead or respawned, the player stands: the flight is over and he plans as anyone does. A
  // caller that set a new goal left the pilot idle: the flight's mode is put back.)
  if (p.airborne && p.escape_on >= 0 && !s.onground && p.k + 1 < p.route.size()) {
    const NavLink* l = link_of(*p.g, p.route[p.k], p.route[p.k + 1]);
    if (l != nullptr && l->escape == p.escape_on) {
      p.goal = goal;
      p.plan_on_landing = true;
      p.mode = PilotMode::air;
      return true;
    }
  }
  p.goal = goal;
  p.plan_on_landing = false;
  if (s.onground || s.waterlevel == 0) {
    // Out of the liquid: none under way (a flight that just landed out of it counted).
    if (p.airborne && p.escape_on >= 0 && s.onground && landed_out(p, s)) p.stats.escapes_out++;
    p.escape_on = -1;
  }
  p.k = 0;
  p.strafe_end_node = -1;
  p.strafe_side = 0;
  p.stats.plans++;
  restart(p, s);
  p.via_k = -1;
  p.route = plan_way(p, plan_from(p, s), goal).nodes;
  if (p.set.gap_near && !p.route.empty()) {
    p.near_cut = gap_too_near(p, s, p.route);
    if (p.near_cut.from >= 0) {
      std::vector<int32_t> round = plan_way(p, plan_from(p, s), goal).nodes;
      if (!round.empty()) {
        p.route = std::move(round);
        p.stats.gap_near++;
      }
      p.near_cut.from = -1;
    }
  }
  if (p.route.empty()) {
    p.mode = PilotMode::idle;
    return false;
  }
  if (p.mode == PilotMode::hop) {
    p.hop_end_why = "plan";
    p.hop_end_at = "";
  }
  p.mode = p.route.size() == 1 ? PilotMode::arrived : PilotMode::walk;
  return true;
}

bool pilot_chain(Pilot& p, int32_t next, Vec3f at) {
  if (p.route.empty() || next < 0 || next == p.goal || p.route.back() != p.goal) return false;
  if (p.mode == PilotMode::idle) return false;
  const std::vector<int32_t> on = plan_way(p, p.goal, next).nodes;
  if (on.size() < 2) return false;
  p.via_k = static_cast<int32_t>(p.route.size()) - 1;
  p.via_at = at;
  p.via_near = 1e30f;
  p.route.insert(p.route.end(), on.begin() + 1, on.end());
  p.goal = next;
  if (p.mode == PilotMode::arrived) p.mode = PilotMode::walk;
  p.stats.vias++;
  return true;
}

bool pilot_retarget(Pilot& p, int32_t goal, const PilotSense& s) {
  // Only a walk or a hop on a walk's link: anything else (a jump, a mover, a swim, a hop start
  // being played, a gap's run) is planned again as it always was.
  const bool moving =
      p.mode == PilotMode::walk || p.mode == PilotMode::hop || p.mode == PilotMode::strafe;
  if (!moving || p.k + 1 >= p.route.size() || p.recipe >= 0 || p.gap_way >= 0 || p.gap_flight ||
      p.escape_on >= 0 || p.plan_on_landing || (s.waterlevel >= 2 && !s.onground))
    return pilot_go(p, goal, s);
  const NavLink* l = link_of(*p.g, p.route[p.k], p.route[p.k + 1]);
  if (l == nullptr || (l->kind != NavKind::walk && l->kind != NavKind::drop) || l->mover >= 0)
    return pilot_go(p, goal, s);
  const int32_t ahead = p.route[p.k + 1];
  if (goal == ahead) return pilot_go(p, goal, s);
  const NavRoute on = plan_way(p, ahead, goal);
  const NavRoute fresh = plan_way(p, plan_from(p, s), goal);
  if (on.nodes.size() < 2 || fresh.nodes.empty()) return pilot_go(p, goal, s);
  // The node ahead first: at the speed held (a walk's at the least), no slower than from here.
  const float speed = std::max(across({}, s.velocity), 0.5f * s.maxspeed);
  const float to_ahead = across(s.origin, node(*p.g, ahead).origin) / speed;
  if (to_ahead + on.time_s > fresh.time_s + kRetargetSlack_s) return pilot_go(p, goal, s);
  p.route.resize(p.k + 2);
  p.route.insert(p.route.end(), on.nodes.begin() + 1, on.nodes.end());
  p.goal = goal;
  p.via_k = -1;
  p.plan_on_landing = false;
  p.stats.retargets++;
  // The stretch read again on the new way ahead (its nodes past the one ahead are new).
  if (p.mode == PilotMode::hop) {
    const int32_t j = hop_stretch(p, s.origin);
    if (j < 0) {
      if (s.onground) {
        p.hop_end_why = "restretch";
        p.hop_end_at = "";
        restart(p, s);
        p.mode = PilotMode::walk;
      } else {
        p.strafe_end_node = static_cast<int32_t>(p.k) + 1;  // landed, it walks on from there
      }
    } else {
      p.strafe_end_node = j;
      p.gap_k = gap_in(p, j);
    }
  } else if (p.mode == PilotMode::strafe) {
    const int32_t j = strafe_stretch(p, s.origin);
    if (j < 0) {
      restart(p, s);
      p.mode = PilotMode::walk;
    } else {
      p.strafe_end_node = j;
    }
  }
  p.best = 1e30f;
  p.best_s = static_cast<float>(p.stats.time_s);
  return true;
}

void pilot_unchain(Pilot& p) {
  if (p.via_k < 0 || static_cast<size_t>(p.via_k) <= p.k) return;
  p.route.resize(static_cast<size_t>(p.via_k) + 1);
  p.goal = p.route.back();
  if (p.strafe_end_node > p.via_k) p.strafe_end_node = p.via_k;
  p.via_k = -1;
}

}  // namespace ge::shooterbots
