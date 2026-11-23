// The way through a campaign map (way_through.hpp): the steps read once, the plan twice a second.
//
// ge:summary The way through a map: its exit, buttons, keys and triggers read; the plan
// ge:keywords way_init way_plan exit button key trigger explore barred door reach
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooterbots/way_through.hpp"
#include "shooterbots/way_bosses.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>

#include "brush/map.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/map_spec.hpp"
#include "shooter/trace.hpp"
#include "shooter/usercmd.hpp"
#include "shooterbots/buttons.hpp"
#include "shooterbots/entities.hpp"

namespace sb = ge::shooterbots;

namespace ge::shooterbots {

namespace {

constexpr int kKeySilver = 131072, kKeyGold = 262144;  // the keys' item bits
constexpr float kButtonClose = 320;  // at a button's spot this near, he presses it, units
constexpr double kPressFor_s = 4;    // and gives the press this long
constexpr float kWayShotFrom = 600;  // (`complete`) a secret wall is shot from this near, units
constexpr float kWayDryShot = 700;   // (`complete`) a monster in the water is shot from this near
constexpr double kWayRide_s = 12;    // the longest ride a plan holds through, s
constexpr float kWayAfloat = 768;    // how far the plan looks for a node to start from in the air

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

sh::Vec3f origin_of(const char* org) {
  char* end = nullptr;
  sh::Vec3f o;
  o.x = std::strtof(org, &end);
  o.y = std::strtof(end, &end);
  o.z = std::strtof(end, &end);
  return o;
}

int edict_of(const ge::shooterbots::BotView& s, const char* model) {
  for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0 || static_cast<size_t>(d.modelindex) >= s.models.size())
      continue;
    if (s.models[static_cast<size_t>(d.modelindex)] == model) return e;
  }
  return -1;
}

// Quake's doors that touch move as one, and a group with a name opens only to what fires it: an
// unnamed leaf of a named group is as shut as its named mate (e4m1's flood gate, opened by the
// button that fires its partner, kept him pressing on it for the whole map). Each unnamed leaf
// takes its group's name. Quake II pairs doors by an explicit team instead.
void name_linked_doors(const ge::shooterbots::BotView& s, const ge::shooter::MapSpec& spec,
                       std::vector<sb::EntityBlock>& blocks) {
  if (spec.brushes && spec.brushes->family != ge::brush::Family::v29 &&
      spec.brushes->family != ge::brush::Family::bsp2)
    return;
  std::vector<size_t> door_block;
  std::vector<int> door_ent;
  for (size_t i = 0; i < blocks.size(); i++) {
    const char* dc = sb::block_value(blocks[i], "classname");
    const char* dm = sb::block_value(blocks[i], "model");
    const char* sf = sb::block_value(blocks[i], "spawnflags");
    if (!dc || !dm || std::strcmp(dc, "func_door") != 0) continue;
    if (sf && (std::atoi(sf) & 4) != 0) continue;  // marked never to link
    const int e = edict_of(s, dm);
    if (e <= 0 || static_cast<size_t>(e) >= s.sv.edicts.size()) continue;
    door_block.push_back(i);
    door_ent.push_back(e);
  }
  std::vector<size_t> group(door_block.size());
  for (size_t i = 0; i < group.size(); i++) group[i] = i;
  const auto root = [&](size_t i) {
    while (group[i] != i) i = group[i] = group[group[i]];
    return i;
  };
  constexpr float kTouch = 2;  // boxes this near count as touching
  for (size_t i = 0; i < door_ent.size(); i++)
    for (size_t j = i + 1; j < door_ent.size(); j++) {
      const sh::Edict& a = s.sv.edicts[static_cast<size_t>(door_ent[i])];
      const sh::Edict& b = s.sv.edicts[static_cast<size_t>(door_ent[j])];
      if (a.absmin.x <= b.absmax.x + kTouch && b.absmin.x <= a.absmax.x + kTouch &&
          a.absmin.y <= b.absmax.y + kTouch && b.absmin.y <= a.absmax.y + kTouch &&
          a.absmin.z <= b.absmax.z + kTouch && b.absmin.z <= a.absmax.z + kTouch)
        group[root(i)] = root(j);
    }
  std::vector<std::string> group_name(door_block.size());
  for (size_t i = 0; i < door_block.size(); i++) {
    const char* dn = sb::block_value(blocks[door_block[i]], "targetname");
    if (dn && group_name[root(i)].empty()) group_name[root(i)] = dn;
  }
  for (size_t i = 0; i < door_block.size(); i++) {
    sb::EntityBlock& b = blocks[door_block[i]];
    if (!sb::block_value(b, "targetname") && !group_name[root(i)].empty())
      b.pairs.emplace_back("targetname", group_name[root(i)]);
  }
}

// A player's box (origin at its middle, 16 across, 24 down and 32 up) touches the box.
bool touches(sh::Vec3f p, sh::Vec3f lo, sh::Vec3f hi) {
  return p.x + 16 >= lo.x && p.x - 16 <= hi.x && p.y + 16 >= lo.y && p.y - 16 <= hi.y &&
         p.z + 32 >= lo.z && p.z - 24 <= hi.z;
}

// A turning door this far or less over an exit is its hatch, units.
constexpr float kHatchOver = 256;

// A trigger hung in a shaft is reached by a fall through it onto a node this far under it, units.
constexpr float kUnderShaft = 512;

// The node a player standing on touches the box from, nearest its middle; else the nearest a step
// beyond it; else one under it that a fall through it lands on; else none.
int32_t node_in(const sb::NavGraph& g, sh::Vec3f lo, sh::Vec3f hi) {
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

// Where a brush entity's model stood when the map spawned it (its `origin`, else the map's own):
// a level entered again keeps its doors and buttons where they were left.
sh::Vec3f spawned_at(const sb::EntityBlock& b) {
  const char* o = sb::block_value(b, "origin");
  return o ? origin_of(o) : sh::Vec3f{};
}

// Whether locked door `k` has moved off where it stood shut (a turning bridge: turned; a bridge
// slid out of the floor, `kLockedOut`: all the way out to where the map drew it, `locked_at`, so
// one still rising or sinking again, as e2m6's pool bridges do on their clock, is still shut).
bool locked_moved(const WayThrough& w, const ge::shooterbots::BotView& s, size_t k) {
  const sh::Edict& d = s.sv.edicts[static_cast<size_t>(w.locked[k])];
  const uint8_t kind = k < w.locked_turns.size() ? w.locked_turns[k] : 0;
  if (kind == kLockedOut) return dist(d.origin, w.locked_at[k]) < 1;
  return dist(kind != 0 ? d.angles : d.origin, w.locked_at[k]) >= 1;
}

// Every node a player at `from` gets to past no door in `barred` (nav_reach with the doors), on
// the links his `jumper` takes (no rocket jump without a launcher: base1's exit, reached by one
// down the lift's shaft, stood him still at the start with no route the pilot would take).
// A link from `a` to `b` goes through the wall from `lo` to `hi`: it passes the wall's middle on
// its thin axis (x or y), the line meets the wall's width there, and a player at either end
// stands at the wall's height (a drop through a hidden lift's doorway starts level with it).
bool crosses(sh::Vec3f a, sh::Vec3f b, sh::Vec3f lo, sh::Vec3f hi) {
  const bool thin_x = hi.x - lo.x <= hi.y - lo.y;
  const float a0 = thin_x ? a.x : a.y, b0 = thin_x ? b.x : b.y;
  const float l0 = thin_x ? lo.x : lo.y, h0 = thin_x ? hi.x : hi.y;
  // Over the wall's middle on its thin axis (a node in front of its face, in the slab the nav
  // gives the mover, is still this side of it).
  const float mid = (l0 + h0) * 0.5f;
  if (!((a0 < mid && b0 >= mid) || (a0 > mid && b0 <= mid))) return false;
  const float t = (mid - a0) / (b0 - a0);
  const float across = thin_x ? a.y + (b.y - a.y) * t : a.x + (b.x - a.x) * t;
  const float l1 = thin_x ? lo.y : lo.x, h1 = thin_x ? hi.y : hi.x;
  if (across < l1 - 15 || across > h1 + 15) return false;
  const auto level = [&](float z) { return z + 32 >= lo.z && z - 24 <= hi.z; };
  return level(a.z) || level(b.z);
}

// The links shut while the doors in `barred` stand shut: those crossing a secret wall among them.
void shut_crossings(WayThrough& w, const std::vector<int32_t>& barred, size_t links) {
  w.link_shut.assign(links, 0);
  for (const WayThrough::Crossing& c : w.crossings)
    for (const int32_t b : barred)
      if (b == c.door) w.link_shut[static_cast<size_t>(c.link)] = 1;
  for (const int32_t li : w.hatch_shut) w.link_shut[static_cast<size_t>(li)] = 1;
}

bool is_wall(const WayThrough& w, int32_t e) {
  for (const int x : w.walls)
    if (x == e) return true;
  return false;
}

// A shut door whose own button's place is on its top (a lift at rest, called from on it): the
// pilot takes him onto it.
bool is_lift(const WayThrough& w, const Fighter& f, int32_t e) {
  for (const WayStep& st : w.steps)
    if (st.kind == WayKind::button && st.node >= 0 && !w.on_top.empty() &&
        w.on_top[static_cast<size_t>(st.node)] &&
        f.button_spots[static_cast<size_t>(st.button)].node == st.node)
      for (const int d : st.doors)
        if (d == e) return true;
  return false;
}

// Whether link `li` is open to him past the doors in `barred` (after shut_crossings).
bool link_open(const sb::NavGraph& g, size_t li, const std::vector<int32_t>& barred,
               const std::vector<int32_t>& node_door, const sb::NavJumper& jumper,
               const WayThrough* w) {
  const sb::NavLink& l = g.links[li];
  if (!jumper.takes(l)) return false;
  if (w && !w->link_shut.empty() && w->link_shut[li]) return false;
  const int32_t mv = g.nodes[static_cast<size_t>(l.to)].mover;
  const int32_t in = mv >= 0 ? mv : node_door[static_cast<size_t>(l.to)];  // or a bridge's
  // Standing on a shut door's top (a lift at rest, its button beside him) is no way through it.
  const bool on_top = w && !w->on_top.empty() && w->on_top[static_cast<size_t>(l.to)];
  for (const int32_t b : barred) {
    if (w && is_wall(*w, b)) continue;  // a shut door's crossings alone (shut_crossings)
    // Its top is a way only for a lift called from on it: a plat that waits for a trigger up
    // top took him down into e1m3's pit by a ride that never came.
    bool lift = false;
    if (w)
      for (const int x : w->lifts) lift = lift || x == b;
    const bool top = on_top && lift;
    if ((l.mover == b && !top) || (in == b && !top)) return false;
  }
  return true;
}

void reach_past(const sb::NavGraph& g, int32_t from, const std::vector<int32_t>& barred,
                const std::vector<int32_t>& node_door, const sb::NavJumper& jumper,
                ge::ai::Search& search, WayThrough* w = nullptr) {
  if (w) shut_crossings(*w, barred, g.links.size());
  search.reset(g.nodes.size());
  if (from < 0) return;
  search.seed(from);
  search.run_all([&](int32_t a) {
    const sb::NavNode& n = g.nodes[static_cast<size_t>(a)];
    for (int32_t k = 0; k < n.links; k++) {
      const size_t li = static_cast<size_t>(n.first_link) + static_cast<size_t>(k);
      if (!link_open(g, li, barred, node_door, jumper, w)) continue;
      const sb::NavLink& l = g.links[li];
      const double cost = search.cost(a) + l.time_s;
      if (search.improves(l.to, cost)) search.reach(l.to, cost, a);
    }
  });
}

// The nodes he gets back to `to` from, on the open links (the reverse of reach_past, the crossings
// as reach_past last left them): `w.returns`.
void reach_back(const sb::NavGraph& g, int32_t to, const std::vector<int32_t>& barred,
                const std::vector<int32_t>& node_door, const sb::NavJumper& jumper, WayThrough& w) {
  const size_t nn = g.nodes.size();
  if (w.rev_first.size() != nn + 1) {  // the incoming links of each node, built once
    w.rev_first.assign(nn + 1, 0);
    for (const sb::NavLink& l : g.links) w.rev_first[static_cast<size_t>(l.to) + 1]++;
    for (size_t n = 0; n < nn; n++) w.rev_first[n + 1] += w.rev_first[n];
    w.rev_link.assign(g.links.size(), 0);
    std::vector<int32_t> fill(w.rev_first.begin(), w.rev_first.end() - 1);
    for (size_t a = 0; a < nn; a++) {
      const sb::NavNode& n = g.nodes[a];
      for (int32_t k = 0; k < n.links; k++) {
        const int32_t li = n.first_link + k;
        w.rev_link[static_cast<size_t>(
            fill[static_cast<size_t>(g.links[static_cast<size_t>(li)].to)]++)] = li;
      }
    }
    w.rev_from.assign(g.links.size(), 0);
    for (size_t a = 0; a < nn; a++) {
      const sb::NavNode& n = g.nodes[a];
      for (int32_t k = 0; k < n.links; k++)
        w.rev_from[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)] =
            static_cast<int32_t>(a);
    }
  }
  w.returns.assign(nn, 0);
  if (to < 0) return;
  w.back_stack.clear();
  w.returns[static_cast<size_t>(to)] = 1;
  w.back_stack.push_back(to);
  while (!w.back_stack.empty()) {
    const int32_t b = w.back_stack.back();
    w.back_stack.pop_back();
    for (int32_t r = w.rev_first[static_cast<size_t>(b)];
         r < w.rev_first[static_cast<size_t>(b) + 1]; r++) {
      const int32_t li = w.rev_link[static_cast<size_t>(r)];
      const int32_t a = w.rev_from[static_cast<size_t>(li)];
      if (w.returns[static_cast<size_t>(a)]) continue;
      if (!link_open(g, static_cast<size_t>(li), barred, node_door, jumper, &w)) continue;
      w.returns[static_cast<size_t>(a)] = 1;
      w.back_stack.push_back(a);
    }
  }
}

// What his way to `node` looks like from where he stands, for the plan's log: the fighter's and the
// pilot's state, what a straight trace to the next node meets, the next links.
void say_way(WayThrough& w, const Fighter& f, const sb::NavGraph& g,
             const ge::shooterbots::BotView& s, int me, sh::Vec3f self, int32_t node,
             const char* what) {
  // Where the way he could not take goes from here: its next links.
  ge::ai::Search rs;
  const sb::NavRoute route = sb::nav_route(g, sb::nav_nearest(g, self), node, rs, 1, w.barred);
  const sb::Pilot& pl = f.bot.pilot.pilot;
  char head[200];
  std::snprintf(head, sizeof head,
                "  %s (fight %s, pilot %s, goal %d, route %zu at %zu, wait %.1f, lost %d); "
                "the way: ",
                what, fight_mode_name(f.mode), sb::pilot_mode_name(pl.mode), f.goal_node,
                pl.route.size(), pl.k, static_cast<double>(pl.wait_s), f.lost_to);
  std::string line = head;
  if (route.nodes.size() > 1) {
    const sh::Vec3f to = g.nodes[static_cast<size_t>(route.nodes[1])].origin;
    const sh::HullTrace tr = sh::trace_move(s.sv, self, {-16, -16, -24}, {16, 16, 32}, to, 0, me);
    char tb[160];
    const sh::Edict* be = tr.ent > 0 ? &s.sv.edicts[static_cast<size_t>(tr.ent)] : nullptr;
    const sh::Edict& me_e = s.sv.edicts[static_cast<size_t>(me)];
    std::snprintf(tb, sizeof tb,
                  "[blocked: frac %.2f ent %d model %d solid %d start %d; vel %.0f %.0f %.0f "
                  "flags %d ground %d mt %d] ",
                  static_cast<double>(tr.fraction), tr.ent, be ? be->modelindex : -1,
                  be ? static_cast<int>(be->solid) : -1, tr.startsolid ? 1 : 0,
                  static_cast<double>(me_e.velocity.x), static_cast<double>(me_e.velocity.y),
                  static_cast<double>(me_e.velocity.z), static_cast<int>(me_e.flags),
                  me_e.groundentity, static_cast<int>(me_e.movetype));
    line += tb;
  }
  for (size_t i = 0; i < route.nodes.size() && i < 10; i++) {
    const sb::NavNode& nd = g.nodes[static_cast<size_t>(route.nodes[i])];
    char part[96];
    std::snprintf(part, sizeof part, "(%.0f %.0f %.0f m%d) ", nd.origin.x, nd.origin.y, nd.origin.z,
                  nd.mover);
    line += part;
    if (i + 1 < route.nodes.size())
      for (int32_t k = 0; k < nd.links; k++) {
        const sb::NavLink& l = g.links[static_cast<size_t>(nd.first_link) + static_cast<size_t>(k)];
        if (l.to != route.nodes[i + 1]) continue;
        line += std::string(sb::nav_kind_name(l.kind)) + (l.mover >= 0 ? "/mover " : " ");
        break;
      }
  }
  w.log->push_back(line);
}

}  // namespace

const char* way_kind_name(WayKind k) {
  switch (k) {
    case WayKind::exit:
      return "exit";
    case WayKind::button:
      return "button";
    case WayKind::key:
      return "key";
    case WayKind::trigger:
      return "trigger";
    case WayKind::explore:
      return "explore";
    case WayKind::monster:
      return "monster";
    case WayKind::secret:
      return "secret";
  }
  return "?";
}

void way_init(WayThrough& w, const Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
              const ge::shooter::MapSpec& spec) {
  w.steps.clear();
  w.locked.clear();
  w.plats.clear();
  w.locked_at.clear();
  w.locked_turns.clear();
  w.cars.clear();
  w.cars_at.clear();
  w.sinks.clear();
  w.sinks_at.clear();
  w.turners.clear();
  w.turners_at.clear();
  w.hatches.clear();
  w.barred.clear();
  w.stats = WayStats{};
  w.plan_at = -1;
  w.step_since = 0;
  w.step_for = kWayGiveUp_s;
  w.step = -1;
  w.calling = -1;
  w.busy_at = -kWayForever;
  w.explore = -1;
  w.explore_since = 0;
  w.idle_since = -1;
  w.dive = -1;
  w.out = -1;
  w.no_route_since = -1;
  w.kind = WayKind::explore;
  w.prey = -1;
  w.prey_since = 0;
  w.prey_left.assign(s.sv.edicts.size(), -1);
  w.prey_tries.assign(s.sv.edicts.size(), 0);
  w.prey_spent.assign(s.sv.edicts.size(), 0);
  w.prey_tick = -1;
  const sb::NavGraph& g = nav.graph;
  w.seen.assign(g.nodes.size(), 0);
  w.node_door.assign(g.nodes.size(), -1);
  w.barred_was.clear();
  w.crossings.clear();
  w.walls.clear();
  // The nodes standing on a mover's top (its box under his feet, his middle over it).
  w.on_top.assign(g.nodes.size(), 0);
  for (size_t n = 0; n < g.nodes.size(); n++) {
    const sb::NavNode& nd = g.nodes[n];
    if (nd.mover <= 0 || static_cast<size_t>(nd.mover) >= s.sv.edicts.size()) continue;
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(nd.mover)];
    const sh::Vec3f o = nd.origin;
    w.on_top[n] = o.z - 24 >= d.absmax.z - 4 && o.x >= d.absmin.x && o.x <= d.absmax.x &&
                  o.y >= d.absmin.y && o.y <= d.absmax.y;
  }
  std::vector<sb::EntityBlock> blocks;
  std::string error;
  if (!sb::parse_entity_text(spec.entities.c_str(), blocks, error)) return;
  name_linked_doors(s, spec, blocks);
  // What opens a door only something else opens: a name a shut door answers to, or a counter's,
  // a relay's or a door's whose own target does (two hops).
  const auto names_door = [&](const std::string& t, std::vector<int>& out) {
    bool any = false;
    for (const sb::EntityBlock& d : blocks) {
      const char* dc = sb::block_value(d, "classname");
      const char* dn = sb::block_value(d, "targetname");
      const char* dm = sb::block_value(d, "model");
      const char* sf = sb::block_value(d, "spawnflags");
      if (!dc || !dn || t != dn) continue;
      if ((std::strcmp(dc, "func_door") == 0 && !(sf && (std::atoi(sf) & 1) != 0)) ||
          std::strcmp(dc, "func_door_secret") == 0 || std::strcmp(dc, "func_plat") == 0) {
        any = true;
        const int e = dm ? edict_of(s, dm) : -1;
        if (e >= 0) out.push_back(e);
      } else if (std::strcmp(dc, "func_door_rotating") == 0 && sf && (std::atoi(sf) & 1) != 0) {
        // A turning bridge (map_nav.hpp: turned up until used, then a floor).
        const int e = dm ? edict_of(s, dm) : -1;
        for (const NavBridge& br : nav.bridges)
          if (br.turns && br.ent == e) {
            any = true;
            out.push_back(e);
          }
      } else if (std::strcmp(dc, "func_door_rotating") == 0 && !(sf && (std::atoi(sf) & 1) != 0)) {
        // A turning door (a hatch, a lever): open once it has turned off its rest.
        const int e = dm ? edict_of(s, dm) : -1;
        if (e < 0) continue;
        any = true;
        out.push_back(e);
        if (std::find(w.turners.begin(), w.turners.end(), e) == w.turners.end()) {
          w.turners.push_back(e);
          w.turners_at.push_back(
              {});  // its spawn angles (a level kept from before may have it turned)
        }
      }
    }
    return any;
  };
  const auto opens = [&](const char* t, std::vector<int>& out) {
    if (!t) return false;
    bool any = names_door(t, out);
    for (const sb::EntityBlock& d : blocks) {
      const char* dc = sb::block_value(d, "classname");
      const char* dn = sb::block_value(d, "targetname");
      const char* dt = sb::block_value(d, "target");
      if (!dc || !dn || !dt || std::strcmp(dn, t) != 0) continue;
      // (A door that fires its own target as it opens passes it on too: base2's lever is a
      // `func_door_rotating` whose target is the exit's door.)
      if (std::strcmp(dc, "trigger_counter") == 0 || std::strcmp(dc, "trigger_relay") == 0 ||
          std::strcmp(dc, "trigger_key") == 0 || std::strcmp(dc, "func_door_rotating") == 0 ||
          std::strcmp(dc, "func_door") == 0)
        any = names_door(dt, out) || any;
    }
    return any;
  };
  // A name answered by a key's lock (Quake II's `trigger_key`), and the map a name's level
  // change leads to (Quake II's exits: a trigger whose target is a `target_changelevel`).
  const auto keyed = [&](const char* t) {
    for (const sb::EntityBlock& d : blocks) {
      const char* dc = sb::block_value(d, "classname");
      const char* dn = sb::block_value(d, "targetname");
      if (t && dc && dn && std::strcmp(dn, t) == 0 && std::strcmp(dc, "trigger_key") == 0)
        return true;
    }
    return false;
  };
  // A name a `target_secret` answers to (Quake II's secrets: the trigger that fires one).
  const auto fires_secret = [&](const char* t) {
    for (const sb::EntityBlock& d : blocks) {
      const char* dc = sb::block_value(d, "classname");
      const char* dn = sb::block_value(d, "targetname");
      if (t && dc && dn && std::strcmp(dn, t) == 0 && std::strcmp(dc, "target_secret") == 0)
        return true;
    }
    return false;
  };
  const auto avoided = [&](std::string_view map) {
    for (const std::string& a : w.avoid)
      if (map == a) return true;
    return false;
  };
  const auto level_change = [&](const char* t) -> const char* {
    for (const sb::EntityBlock& d : blocks) {
      const char* dc = sb::block_value(d, "classname");
      const char* dn = sb::block_value(d, "targetname");
      if (t && dc && dn && std::strcmp(dn, t) == 0 && std::strcmp(dc, "target_changelevel") == 0) {
        const char* map = sb::block_value(d, "map");
        return map ? map : "";
      }
    }
    return nullptr;
  };
  bool quake_keys = false;  // the map's keys are Quake's (items), whose doors open to a touch
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    quake_keys = quake_keys || (cn && std::strncmp(cn, "item_key", 8) == 0);
  }
  w.key_doors.clear();
  std::vector<int> gates;  // the shut doors barred by the links through their shut boxes
  // The links that pass through secret wall `e` (shut while it is barred).
  const auto add_crossings = [&](int e) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sb::NavNode& n = g.nodes[a];
      for (int32_t k = 0; k < n.links; k++) {
        const int32_t li = n.first_link + k;
        const sb::NavLink& l = g.links[static_cast<size_t>(li)];
        if (crosses(n.origin, g.nodes[static_cast<size_t>(l.to)].origin, d.absmin, d.absmax))
          w.crossings.push_back({li, static_cast<int32_t>(a), e});
      }
    }
  };
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* model = sb::block_value(b, "model");
    const char* target = sb::block_value(b, "target");
    const char* name = sb::block_value(b, "targetname");
    if (!cn) continue;
    const int m = model && model[0] == '*' ? std::atoi(model + 1) : -1;
    // The brush models' boxes: the spec's hulls, or (Quake II's runner keeps none) the map's.
    const size_t model_count = !spec.models.empty() ? spec.models.size()
                               : spec.brushes       ? spec.brushes->models.size()
                                                    : 0;
    const bool brush = m > 0 && static_cast<size_t>(m) < model_count;
    WayStep st;
    const bool trigger =
        std::strcmp(cn, "trigger_once") == 0 || std::strcmp(cn, "trigger_multiple") == 0;
    if (std::strcmp(cn, "trigger_changelevel") == 0 && brush) {
      st.kind = WayKind::exit;
      const char* map = sb::block_value(b, "map");
      st.name = map ? map : "";
      if (avoided(st.name)) continue;  // a secret level the run leaves for its own
    } else if (w.complete && std::strcmp(cn, "trigger_secret") == 0 && brush) {
      st.kind = WayKind::secret;  // Quake's: walked into, it counts
      st.name = cn;
    } else if (w.complete && trigger && brush && target && !name && fires_secret(target)) {
      st.kind = WayKind::secret;
      st.name = "secret";
    } else if (const char* to = trigger && brush && target ? level_change(target) : nullptr) {
      st.kind = WayKind::exit;  // Quake II's: walked through, it fires the level change
      st.name = to;
      // The way back to a map he has played (base2's start: the car up to base1) is taken only
      // once nothing else is left (base3's way on is back to base2, by another way in).
      std::string_view goes(to);
      if (!goes.empty() && goes[0] == '*') goes.remove_prefix(1);
      goes = goes.substr(0, goes.find('$'));
      for (const std::string& was : w.been) st.revisit = st.revisit || goes == was;
      if (avoided(goes)) continue;
    } else if ((std::strcmp(cn, "trigger_once") == 0 || std::strcmp(cn, "trigger_multiple") == 0) &&
               brush && target && !name) {
      st.kind = WayKind::trigger;  // one fired by something else is not walked into
      st.target = target;
      st.opens = opens(target, st.doors);
      st.keyed = keyed(target);
    } else if (std::strcmp(cn, "item_key1") == 0 || std::strcmp(cn, "item_key2") == 0) {
      const char* org = sb::block_value(b, "origin");
      if (!org) continue;
      st.kind = WayKind::key;
      st.opens = true;
      st.key_bit = cn[8] == '1' ? kKeySilver : kKeyGold;
      const sh::Vec3f o = origin_of(org);
      st.lo = {o.x - 16, o.y - 16, o.z};
      st.hi = {o.x + 16, o.y + 16, o.z + 32};
      st.node = node_in(g, st.lo, st.hi);
      st.name = cn;
      w.steps.push_back(st);
      continue;
    } else if (w.complete && target &&
               (std::strncmp(cn, "weapon_", 7) == 0 || std::strncmp(cn, "ammo_", 5) == 0 ||
                std::strncmp(cn, "item_", 5) == 0)) {
      // (`complete`) A thing that fires its target when taken (base1's shotgun in the east wing:
      // two soldiers come for it): walked over, like a trigger.
      const char* org = sb::block_value(b, "origin");
      if (!org) continue;
      st.kind = WayKind::trigger;
      st.target = target;
      st.opens = opens(target, st.doors);
      const sh::Vec3f o = origin_of(org);
      st.lo = {o.x - 8, o.y - 8, o.z - 8};
      st.hi = {o.x + 8, o.y + 8, o.z + 24};
      st.node = node_in(g, {o.x - 16, o.y - 16, o.z}, {o.x + 16, o.y + 16, o.z + 32});
      st.name = cn;
      w.steps.push_back(st);
      continue;
    } else if (std::strncmp(cn, "key_", 4) == 0) {
      // Quake II's keys: taken when the thing lying there is gone.
      const char* org = sb::block_value(b, "origin");
      if (!org) continue;
      st.kind = WayKind::key;
      st.opens = true;
      const sh::Vec3f o = origin_of(org);
      float best = 64;
      for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
        const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
        if (d.free || d.modelindex <= 0) continue;
        const float dd = dist(d.origin, o);
        if (dd < best) {
          best = dd;
          st.ent = e;
        }
      }
      const sh::Vec3f at = st.ent >= 0 ? s.sv.edicts[static_cast<size_t>(st.ent)].origin : o;
      st.lo = {at.x - 16, at.y - 16, at.z - 16};
      st.hi = {at.x + 16, at.y + 16, at.z + 32};
      st.node = node_in(g, st.lo, st.hi);
      st.name = cn;
      w.steps.push_back(st);
      continue;
    } else if (std::strcmp(cn, "func_plat") == 0 && name && brush) {
      // A plat with a name waits up top for what fires it, and only then runs: shut to a ride
      // till then (e1m3's took him down into a pit by a ride that never came back up).
      const int e = edict_of(s, model);
      if (e >= 0) {
        w.locked.push_back(e);
        w.locked_at.push_back(s.sv.edicts[static_cast<size_t>(e)].origin);
        w.locked_turns.push_back(0);
        w.plats.push_back(e);
      }
      continue;
    } else if (std::strcmp(cn, "func_door_secret") == 0 && brush) {
      // A secret wall stays shut to a walk: its trigger, its button or a shot opens it (the nav's
      // links through it waited there for ever: e1m1's wall by the slime pit stood him still for
      // nine minutes).
      const int e = edict_of(s, model);
      if (e >= 0) {
        w.locked.push_back(e);
        w.walls.push_back(e);
        const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
        w.locked_at.push_back(d.origin);
        w.locked_turns.push_back(0);
        add_crossings(e);
      }
      continue;
    } else if (std::strcmp(cn, "func_door") == 0 && !name && brush && quake_keys &&
               ((std::atoi(sb::block_value(b, "spawnflags") ? sb::block_value(b, "spawnflags")
                                                            : "0") &
                 24) != 0)) {
      // Quake's key door (spawnflag 8 gold, 16 silver: each map's doors pair so with the key it
      // lays, e1m2's silver key with doors of 16, e1m3's gold with doors of 8): shut to him until
      // he holds its key (a route through e1m2's silver doors before the key stood him at them for
      // minutes).
      const int e = edict_of(s, model);
      if (e >= 0) {
        const int sf = std::atoi(sb::block_value(b, "spawnflags"));
        w.locked.push_back(e);
        w.locked_at.push_back(s.sv.edicts[static_cast<size_t>(e)].origin);
        w.locked_turns.push_back(0);
        w.key_doors.push_back({e, (sf & 16) != 0 ? kKeySilver : kKeyGold});
        gates.push_back(e);
      }
      continue;
    } else if (std::strcmp(cn, "func_door") == 0 && name && brush) {
      const char* sf = sb::block_value(b, "spawnflags");
      if (std::strcmp(cn, "func_door") == 0 && sf && (std::atoi(sf) & 1) != 0) {
        // Starts open: a bridge still in is shut to him (its nodes are where it slides out to);
        // any other is open.
        const int e = edict_of(s, model);
        for (const NavBridge& br : nav.bridges)
          if (br.ent == e && name) {
            w.locked.push_back(e);
            w.locked_at.push_back(spawned_at(b));
            w.locked_turns.push_back(kLockedOut);
            for (const int32_t n : br.nodes) w.node_door[static_cast<size_t>(n)] = e;
          }
        continue;
      }
      const int e = edict_of(s, model);
      if (e < 0) continue;
      // A floor that sinks under its button and comes back (the nav rides it from where it
      // sinks to, map_nav's sink lift) is a lift, not a barrier: barred, its top kept him on it for
      // good.
      bool lift = false;
      for (const sb::NavMover& mv : g.movers) lift = lift || (mv.ent == e && sb::ridden(mv));
      if (lift) {
        w.sinks.push_back(e);  // open while away from where it stands (called down)
        w.sinks_at.push_back(spawned_at(b));
        continue;
      }
      // A door whose own button rides inside it is a lift's car, not a barrier (base1's exit and
      // base2's start: a hollow car the button on its inner wall sends along its shaft): it keeps
      // no one out, and barring it kept him from the button that moves it.
      bool holds = false, bars_one = true;
      const sh::Edict& de = s.sv.edicts[static_cast<size_t>(e)];
      for (const sb::NavNode& n : g.nodes) holds = holds || n.mover == e;
      for (const ButtonSpot& bs : f.button_spots) {
        bool its = false;
        for (const int d : bs.doors) its = its || d == e;
        constexpr float kIn = 16;  // the button's middle this near the car's box counts as in it
        if (its && bs.at.x > de.absmin.x - kIn && bs.at.x < de.absmax.x + kIn &&
            bs.at.y > de.absmin.y - kIn && bs.at.y < de.absmax.y + kIn &&
            bs.at.z > de.absmin.z - kIn && bs.at.z < de.absmax.z + kIn) {
          bars_one = false;
          // A car with no node in it (base2's lift down to bunk1: its button is pressed from
          // the node at its doors) holds the one its button is pressed from.
          if (bs.node >= 0) {
            const sh::Vec3f o = g.nodes[static_cast<size_t>(bs.node)].origin;
            holds =
                holds || (o.x > de.absmin.x - kButtonClose && o.x < de.absmax.x + kButtonClose &&
                          o.y > de.absmin.y - kButtonClose && o.y < de.absmax.y + kButtonClose);
          }
        }
      }
      if (holds && !bars_one) {
        // Still watched: the button that sends it is pressed only once it has moved.
        w.cars.push_back(e);
        w.cars_at.push_back(spawned_at(b));
        continue;
      }
      w.locked.push_back(e);
      w.locked_at.push_back(spawned_at(b));
      w.locked_turns.push_back(0);
      gates.push_back(e);
      continue;
    } else if (std::strcmp(cn, "func_door_rotating") == 0 && name && brush) {
      // A turning bridge still turned up is shut to him (its nodes are where it turns down to).
      const int e = edict_of(s, model);
      for (const NavBridge& br : nav.bridges)
        if (br.turns && br.ent == e) {
          w.locked.push_back(e);
          w.locked_at.push_back(br.spawn_angles);
          w.locked_turns.push_back(1);
          for (const int32_t n : br.nodes) w.node_door[static_cast<size_t>(n)] = e;
        }
      continue;
    } else {
      continue;
    }
    if (!spec.models.empty()) {
      st.lo = spec.models[static_cast<size_t>(m)].mins;
      st.hi = spec.models[static_cast<size_t>(m)].maxs;
    } else {
      const ge::brush::Model& bm = spec.brushes->models[static_cast<size_t>(m)];
      st.lo = {bm.mins[0], bm.mins[1], bm.mins[2]};
      st.hi = {bm.maxs[0], bm.maxs[1], bm.maxs[2]};
    }
    st.node = node_in(g, st.lo, st.hi);
    // A trigger flush with the door it opens (base3's key lock, a strip before its door): its node
    // stands where his box meets the shut door, so every way there is through it. The nearest
    // node by the trigger clear of its doors instead.
    if (st.node >= 0 && !st.doors.empty()) {
      const auto meets_door = [&](sh::Vec3f o) {
        for (const int d : st.doors) {
          const sh::Edict& de = s.sv.edicts[static_cast<size_t>(d)];
          if (o.x + 16 > de.absmin.x && o.x - 16 < de.absmax.x && o.y + 16 > de.absmin.y &&
              o.y - 16 < de.absmax.y && o.z + 32 > de.absmin.z && o.z - 24 < de.absmax.z)
            return true;
        }
        return false;
      };
      if (meets_door(g.nodes[static_cast<size_t>(st.node)].origin)) {
        const sh::Vec3f mid = (st.lo + st.hi) * 0.5f, grow{48, 48, 56};
        int32_t best = -1;
        float best_d = 0;
        for (size_t n = 0; n < g.nodes.size(); n++) {
          const sh::Vec3f o = g.nodes[n].origin;
          if (!touches(o, st.lo - grow, st.hi + grow) || meets_door(o)) continue;
          const float d = dist(o, mid);
          if (best < 0 || d < best_d) {
            best = static_cast<int32_t>(n);
            best_d = d;
          }
        }
        if (best >= 0) st.node = best;
      }
    }
    if (st.name.empty()) st.name = cn;
    w.steps.push_back(st);
  }
  for (WayStep& st : w.steps)
    if (st.kind == WayKind::key && st.key_bit != 0)
      for (const WayThrough::KeyDoor& kd : w.key_doors)
        if (kd.bit == st.key_bit) st.doors.push_back(kd.door);
  for (size_t k = 0; k < f.button_spots.size(); k++) {
    const ButtonSpot& bs = f.button_spots[k];
    WayStep st;
    st.kind = WayKind::button;
    st.button = static_cast<int>(k);
    st.lo = st.hi = s.sv.edicts[static_cast<size_t>(bs.ent)].origin;  // where it rests
    st.node = bs.node;
    st.name = bs.shoot ? "shot button" : "button";
    const sh::Edict& be = s.sv.edicts[static_cast<size_t>(bs.ent)];
    const char* target = nullptr;
    for (const sb::EntityBlock& b : blocks) {
      const char* model = sb::block_value(b, "model");
      if (model && ((be.modelindex > 0 && static_cast<size_t>(be.modelindex) < s.models.size() &&
                     s.models[static_cast<size_t>(be.modelindex)] == model) ||
                    (!bs.model.empty() && bs.model == model))) {
        st.lo = st.hi = spawned_at(b);  // a level kept from before may have it pressed already
        target = sb::block_value(b, "target");
        st.opens = opens(target, st.doors);
      }
    }
    // One whose target, or a relay's it fires, changes the level is an exit pressed (base2's
    // lift down to bunk1: its button fires the level change through a relay).
    {
      const char* t = target;
      for (int hop = 0; t != nullptr && hop < 4 && !st.leaves; hop++) {
        if (const char* to = level_change(t)) {
          st.leaves = true;
          st.name = to;
          std::string_view goes(to);
          if (!goes.empty() && goes[0] == '*') goes.remove_prefix(1);
          goes = goes.substr(0, goes.find('$'));
          for (const std::string& was : w.been) st.revisit = st.revisit || goes == was;
          break;
        }
        const char* next = nullptr;
        for (const sb::EntityBlock& d : blocks) {
          const char* dc = sb::block_value(d, "classname");
          const char* dn = sb::block_value(d, "targetname");
          if (dc && dn && std::strcmp(dn, t) == 0 && std::strcmp(dc, "trigger_relay") == 0)
            next = sb::block_value(d, "target");
        }
        t = next;
      }
    }
    // One that moves water (base3's at the bottom of its pool drains it for minutes, opening a
    // door down there) leaves him at the bottom of a dry pit his swim out of was: pressed last.
    for (const sb::EntityBlock& b : blocks) {
      const char* name = sb::block_value(b, "targetname");
      const char* cls = sb::block_value(b, "classname");
      st.drains =
          st.drains || (target != nullptr && name != nullptr && cls != nullptr &&
                        std::strcmp(name, target) == 0 && std::strcmp(cls, "func_water") == 0);
    }
    if (st.doors.empty() && bs.doors.size() == 1 && bs.doors[0] == bs.ent) {  // a shot wall
      st.doors = bs.doors;
      st.opens = true;
      st.name = "secret wall";
    }
    // Arrived from another map in a car (base2's start: the lift down from base1), the car's
    // button sends him back up to the way back: no step of his.
    bool car_back = false;
    if (!w.been.empty()) {
      const sh::Vec3f at = s.sv.edicts[1].origin;
      for (const int d : st.doors) {
        const sh::Edict& de = s.sv.edicts[static_cast<size_t>(d)];
        car_back = car_back || (at.x > de.absmin.x && at.x < de.absmax.x && at.y > de.absmin.y &&
                                at.y < de.absmax.y && at.z > de.absmin.z && at.z < de.absmax.z);
      }
    }
    if (!car_back) w.steps.push_back(st);
  }
  // (`complete`) Quake's boss the lightning kills: the electrodes, the buttons that move them, the
  // button that fires the bolt and the doors his death opens.
  if (w.complete) way_bolt_init(w, f, s, blocks);
  // The turning doors a step opens that lie over an exit, within kHatchOver above it.
  for (const int h : w.turners) {
    const sh::Edict& he = s.sv.edicts[static_cast<size_t>(h)];
    for (const WayStep& st : w.steps)
      if (st.kind == WayKind::exit && he.absmin.x < st.hi.x && he.absmax.x > st.lo.x &&
          he.absmin.y < st.hi.y && he.absmax.y > st.lo.y && he.absmin.z >= st.hi.z - 8 &&
          he.absmin.z - st.hi.z < kHatchOver &&
          std::find(w.hatches.begin(), w.hatches.end(), h) == w.hatches.end())
        w.hatches.push_back(h);
  }
  // A key lying under or inside a door something else opens (base3's blue key, on a lift under a
  // floor plate that a button slides aside): out of reach until that door has moved.
  for (WayStep& st : w.steps) {
    if (st.kind != WayKind::key) continue;
    const float x = (st.lo.x + st.hi.x) / 2, y = (st.lo.y + st.hi.y) / 2;
    for (size_t k = 0; k < w.locked.size(); k++) {
      const sh::Edict& de = s.sv.edicts[static_cast<size_t>(w.locked[k])];
      if (x > de.absmin.x && x < de.absmax.x && y > de.absmin.y && y < de.absmax.y &&
          de.absmax.z > st.lo.z - kKeyCovered && de.absmin.z < st.hi.z + kKeyCovered)
        st.covers.push_back(static_cast<int>(k));
    }
  }
  // A shut door bars the links that pass through its shut box, not every node of its sweep: the
  // nav gives a sliding door's whole travel as its mover, and that barred the floor in front of
  // e1m2's exit doors where the trigger that opens them stands; a swim link through e4m1's shut
  // flood gates passed on no node of theirs. A lift keeps its mover's rule (its top is where its
  // button is pressed from).
  w.lifts.clear();
  for (const int e : w.locked)
    if (is_lift(w, f, e)) w.lifts.push_back(e);
  for (const int e : gates) {
    if (is_lift(w, f, e)) continue;
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    // A player's box (feet 24 below his origin, head 32 above) overlapping the door's, a hair
    // inside.
    const sh::Vec3f lo = d.absmin + sh::Vec3f{-15, -15, -31}, hi = d.absmax + sh::Vec3f{15, 15, 23};
    const auto hits = [&](sh::Vec3f a, sh::Vec3f b) {
      const float len = dist(a, b);
      const int steps = 1 + static_cast<int>(len / 8);
      for (int i = 0; i <= steps; i++) {
        const float u = static_cast<float>(i) / static_cast<float>(steps);
        const sh::Vec3f p = a + (b - a) * u;
        if (p.x > lo.x && p.x < hi.x && p.y > lo.y && p.y < hi.y && p.z > lo.z && p.z < hi.z)
          return true;
      }
      return false;
    };
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sb::NavNode& n = g.nodes[a];
      for (int32_t k = 0; k < n.links; k++) {
        const int32_t li = n.first_link + k;
        const sb::NavLink& l = g.links[static_cast<size_t>(li)];
        if (hits(n.origin, g.nodes[static_cast<size_t>(l.to)].origin))
          w.crossings.push_back({li, static_cast<int32_t>(a), e});
      }
    }
    w.walls.push_back(e);
  }
  // (`complete`) A trigger or a secret whose node the start does not reach (a strip at the foot of
  // a ledge over water, its node on the ledge): a node he reaches that stands by its box instead.
  if (w.complete) {
    const int32_t start = sb::nav_nearest(g, s.sv.edicts[1].origin);
    ge::ai::Search reach;
    if (start >= 0) sb::nav_reach(g, start, reach);
    for (WayStep& st : w.steps) {
      if ((st.kind != WayKind::trigger && st.kind != WayKind::secret) || start < 0 ||
          (st.node >= 0 && reach.reached(st.node)))
        continue;
      const sh::Vec3f mid = (st.lo + st.hi) * 0.5f, grow{kWayTouchBy, kWayTouchBy, kWayTouchBy};
      int32_t best = -1;
      float best_d = 0;
      for (size_t n = 0; n < g.nodes.size(); n++) {
        const sh::Vec3f o = g.nodes[n].origin;
        if (!reach.reached(static_cast<int32_t>(n)) || !touches(o, st.lo - grow, st.hi + grow))
          continue;
        const float d = dist(o, mid);
        if (best < 0 || d < best_d) {
          best = static_cast<int32_t>(n);
          best_d = d;
        }
      }
      if (best < 0) continue;
      st.node = best;
      st.reach_in = true;
    }
    // One no node stands by, in a crawl's cell (a nook under a floor): crawled to from the end of
    // that crawl he reaches, the nearest way.
    for (WayStep& st : w.steps) {
      if ((st.kind != WayKind::trigger && st.kind != WayKind::secret) || st.reach_in || start < 0 ||
          (st.node >= 0 && touches(g.nodes[static_cast<size_t>(st.node)].origin, st.lo, st.hi)))
        continue;
      for (const MapCrawl& cr : nav.crawls) {
        for (size_t c = 0; c < cr.cells.size(); c++) {
          if (!touches(cr.cells[c].at, st.lo, st.hi)) continue;
          for (const auto& [e, n] : cr.ends) {
            if (!reach.reached(n)) continue;
            std::vector<sh::Vec3f> way = crawl_path(cr, e, static_cast<int>(c));
            if (!st.crawl.empty() && way.size() >= st.crawl.size()) continue;
            st.crawl = std::move(way);
            st.node = n;
          }
          break;
        }
      }
      st.reach_in = !st.crawl.empty();
    }
  }
}

void way_complete_init(Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
                       const ge::shooter::MapSpec& spec) {
  std::vector<sb::EntityBlock> blocks;
  std::string error;
  if (!sb::parse_entity_text(spec.entities.c_str(), blocks, error)) return;
  name_linked_doors(s, spec, blocks);
  const sb::NavGraph& g = nav.graph;
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* model = sb::block_value(b, "model");
    const char* name = sb::block_value(b, "targetname");
    const char* health = sb::block_value(b, "health");
    const char* sf = sb::block_value(b, "spawnflags");
    if (!cn || !model || model[0] != '*') continue;
    // Quake's secret wall opens to a shot unless it has a name (then its button or trigger opens
    // it) and is not marked to answer a shot anyway (spawnflag 16); a plain door does when it has
    // health and no name.
    const int flags = sf ? std::atoi(sf) : 0;
    const bool secret = std::strcmp(cn, "func_door_secret") == 0 && (!name || (flags & 16) != 0);
    const bool shot_door =
        std::strcmp(cn, "func_door") == 0 && !name && health && std::atof(health) > 0;
    if (!secret && !shot_door) continue;
    const int e = edict_of(s, model);
    if (e < 0) continue;
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    ButtonSpot bs;
    bs.ent = e;
    bs.at = (d.absmin + d.absmax) * 0.5f;
    bs.shoot = true;
    bs.doors = {e};
    // Shot from the nearest node within kWayShotFrom whose eye line meets the wall itself.
    float best_d = kWayShotFrom;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      const float dd = dist(o, bs.at);
      if (dd >= best_d) continue;
      const sh::HullTrace tr =
          sh::trace_move(s.sv, {o.x, o.y, o.z + 22}, {}, {}, bs.at, sh::kTraceNoMonsters, -1);
      if (tr.fraction < 1 && tr.ent != e) continue;
      bs.node = static_cast<int32_t>(n);
      best_d = dd;
    }
    if (bs.node < 0) continue;  // seen from nowhere he stands
    f.button_spots.push_back(bs);
  }
  way_shot_triggers(f, nav, s, spec, blocks);
}

void way_plan(WayThrough& w, Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
              int me, double t) {
  const sb::NavGraph& g = nav.graph;
  const sh::Vec3f self = s.sv.edicts[static_cast<size_t>(me)].origin;
  // At a button of the plan: pressed (or shot) by the fighter's own hands.
  if (w.step >= 0 && w.kind == WayKind::button) {
    const WayStep& st = w.steps[static_cast<size_t>(w.step)];
    const ButtonSpot& bs = f.button_spots[static_cast<size_t>(st.button)];
    if (bs.node >= 0 && dist(g.nodes[static_cast<size_t>(bs.node)].origin, self) < kButtonClose &&
        (f.poke != st.button || t >= f.poke_until)) {
      f.poke = st.button;
      f.poke_until = t + kPressFor_s;
    }
  }
  if (w.plan_at >= 0 && t - w.plan_at < kWayPlan_s) return;
  // Riding a mover the plan holds (on its way up, the nearest node is the plat's own, whose links
  // reach nothing: e1m3's plat, planned away from at every half second, walked him off mid-ride
  // for minutes), at most kWayRide_s.
  const sh::Edict& me_e = s.sv.edicts[static_cast<size_t>(me)];
  const bool riding = f.bot.pilot.pilot.mode == sb::PilotMode::ride &&
                      (me_e.flags & sh::kFlagOnGround) != 0 && me_e.groundentity > 0 &&
                      f.objective >= 0;
  if (!riding) w.ride_since = -1;
  if (riding && w.ride_since < 0) w.ride_since = t;
  if (riding && t - w.ride_since < kWayRide_s) return;
  w.plan_at = t;
  w.stats.plans++;
  // Where he stands now is explored.
  for (size_t n = 0; n < g.nodes.size(); n++)
    if (!w.seen[n] && dist(g.nodes[n].origin, self) < kWaySeen) w.seen[n] = 1;

  // What is done.
  const sb::Stats stats_now = read_stats(s, me, *f.arsenal);
  const int items = stats_now.items;
  const bool blasts = sb::can_blast(stats_now);
  const auto door_open = [&](int ent) {
    for (size_t k = 0; k < w.locked.size(); k++)
      if (w.locked[k] == ent) return locked_moved(w, s, k);
    for (size_t k = 0; k < w.cars.size(); k++)
      if (w.cars[k] == ent)
        return dist(s.sv.edicts[static_cast<size_t>(ent)].origin, w.cars_at[k]) >= 1;
    for (size_t k = 0; k < w.turners.size(); k++)
      if (w.turners[k] == ent)
        return dist(s.sv.edicts[static_cast<size_t>(ent)].angles, w.turners_at[k]) >= 1;
    for (size_t k = 0; k < w.sinks.size(); k++)
      if (w.sinks[k] == ent)
        return dist(s.sv.edicts[static_cast<size_t>(ent)].origin, w.sinks_at[k]) >= 1;
    return true;  // no door of the plan's
  };
  for (WayStep& st : w.steps) {
    if (st.kind == WayKind::button) {
      // A button that opens doors is done while they stand open (one that shuts again, a lift
      // called, is pressed again); one that opens none (a counter's) once it is pressed.
      const ButtonSpot& bs = f.button_spots[static_cast<size_t>(st.button)];
      const bool pressed = dist(s.sv.edicts[static_cast<size_t>(bs.ent)].origin, st.lo) > 0.5f;
      bool all = !bs.doors.empty();
      for (const int d : bs.doors) all = all && door_open(d);
      const bool now = pressed || all || (bs.doors.empty() && st.done);
      if (now && !st.done) w.stats.steps_done++;
      st.done = now;
      continue;
    }
    const bool again = st.kind == WayKind::trigger && w.complete && st.opens && !st.doors.empty();
    if (st.done && !again) continue;
    const bool was_done = st.done;
    switch (st.kind) {
      case WayKind::key:
        if (st.ent >= 0) {
          // Once gone, taken for good: its slot is free for the next thing spawned (a shot, a
          // gib), which would bring the key back as one not taken.
          const sh::Edict& k = s.sv.edicts[static_cast<size_t>(st.ent)];
          st.done = st.done || k.free || k.modelindex <= 0;
        } else {
          st.done = (items & st.key_bit) != 0;
        }
        break;
      case WayKind::secret:
        st.done = touches(self, st.lo, st.hi);
        break;
      case WayKind::trigger:
        // Walked through with the key held, or one that opens a secret wall (shut again after a
        // while: e1m1's hidden plat's, the way out of its pit): done while its doors stand open.
        if ((st.keyed || again) && !st.doors.empty()) {
          bool open = true;
          for (const int d : st.doors) open = open && door_open(d);
          st.done = open;
        } else {
          st.done = touches(self, st.lo, st.hi);
        }
        break;
      default:
        break;
    }
    if (st.done && !was_done) w.stats.steps_done++;
    if (st.done && !was_done && !st.crawl.empty()) {  // back the way he crawled in
      w.crawl_out.assign(st.crawl.rbegin(), st.crawl.rend());
      if (st.node >= 0) w.crawl_out.push_back(g.nodes[static_cast<size_t>(st.node)].origin);
      w.crawl_out_at = t;
    }
  }
  // A waiting plat once moved runs for good: never locked again (where it stood is put out of
  // reach).
  for (size_t k = 0; k < w.locked.size(); k++) {
    bool plat = false;
    for (const int p : w.plats) plat = plat || p == w.locked[k];
    if (plat && locked_moved(w, s, k)) w.locked_at[k] = {1e9f, 1e9f, 1e9f};
  }
  // The drops through a floor door still resting over its hole.
  w.hatch_shut.clear();
  for (const MapNav::Hatch& h : nav.hatches)
    if (dist(s.sv.edicts[static_cast<size_t>(h.door)].origin, h.rest) < 1)
      w.hatch_shut.push_back(h.link);
  // The doors shut now that only something else opens.
  w.barred.clear();
  for (size_t k = 0; k < w.locked.size(); k++) {
    if (locked_moved(w, s, k)) continue;
    bool held = false;  // a key door with its key held opens to his touch
    for (const WayThrough::KeyDoor& kd : w.key_doors)
      held = held || (kd.door == w.locked[k] && (items & kd.bit) != 0);
    if (!held && t >= w.hopeful_until) w.barred.push_back(w.locked[k]);
  }
  // His own routes never through them either (a secret wall by its crossings, below: its sweep's
  // movers stood him still in front of it).
  f.bot.pilot.pilot.shut.clear();
  for (const int32_t b : w.barred)
    if (!is_wall(w, b) && !is_lift(w, f, b)) f.bot.pilot.pilot.shut.push_back(b);
  f.bot.pilot.pilot.hollow = w.cars;  // and a car at rest is walked through
  if (w.barred != w.barred_was || w.hatch_shut != w.hatch_shut_was) {  // nor onto a bridge still in
    w.barred_was = w.barred;
    w.hatch_shut_was = w.hatch_shut;
    std::vector<sb::NavExtraLink>& sl = f.bot.pilot.pilot.shut_links;
    sl.clear();
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sb::NavNode& n = g.nodes[a];
      for (int32_t k = 0; k < n.links; k++) {
        const sb::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
        const int32_t d = w.node_door[static_cast<size_t>(l.to)];
        if (d < 0) continue;
        for (const int32_t b : w.barred)
          if (b == d) sl.push_back({static_cast<int32_t>(a), l.to, l.kind, l.time_s});
      }
    }
    for (const WayThrough::Crossing& c : w.crossings)  // nor past a secret wall still shut
      for (const int32_t b : w.barred)
        if (b == c.door) {
          const sb::NavLink& l = g.links[static_cast<size_t>(c.link)];
          sl.push_back({c.from, l.to, l.kind, l.time_s});
        }
    for (const MapNav::Hatch& h : nav.hatches)  // nor through a floor door still resting
      for (const int32_t li : w.hatch_shut)
        if (li == h.link) {
          const sb::NavLink& l = g.links[static_cast<size_t>(li)];
          sl.push_back({h.from, l.to, l.kind, l.time_s});
        }
  }
  int32_t from = sb::nav_nearest(g, self);
  // None near (in the air, falling: `end` starts him high over its water): the nearest node at all
  // within kWayAfloat, the one below him first, so the plan has somewhere to start.
  if (from < 0) {
    float best = kWayAfloat;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      const float d = dist(o, self) + (o.z > self.z ? kWayAfloat * 0.5f : 0.0f);
      if (d < best) {
        best = d;
        from = static_cast<int32_t>(n);
      }
    }
  }
  // A place to look at is dry ground he can walk back from: the water is seen from its banks
  // (exploring it cell by cell took him down a flooded tunnel in base1 until he stopped at its end
  // under water), and a trap (map_nav.hpp) only from above (base1's east pool, dropped into at 94
  // s, held him to the end). `no_place`: a node that is neither.
  const auto no_place = [&](size_t n) {
    return (n < nav.wet.size() && nav.wet[n] != 0) || (n < nav.trap.size() && nav.trap[n] != 0);
  };
  // What each shut door would open up: the places not yet seen, or an exit, that only its opening
  // reaches (worked out with it open and the rest as they are).
  w.gain.assign(w.locked.size(), 0);
  w.gain_back.assign(w.locked.size(), 0);
  {
    reach_past(g, from, w.barred, w.node_door, f.bot.pilot.pilot.set.jumper, w.search, &w);
    std::vector<uint8_t> now(g.nodes.size(), 0);
    for (size_t n = 0; n < g.nodes.size(); n++) now[n] = w.search.reached(static_cast<int32_t>(n));
    std::vector<int32_t> others;
    for (size_t k = 0; k < w.locked.size(); k++) {
      bool shut_now = false;
      others.clear();
      for (const int32_t b : w.barred) {
        if (b == w.locked[k])
          shut_now = true;
        else
          others.push_back(b);
      }
      if (!shut_now) continue;
      reach_past(g, from, others, w.node_door, f.bot.pilot.pilot.set.jumper, w.search, &w);
      int64_t more = 0, back = 0;
      for (size_t n = 0; n < g.nodes.size(); n++) {
        if (now[n] || !w.search.reached(static_cast<int32_t>(n))) continue;
        back++;
        if (!w.seen[n] && !no_place(n)) more++;
      }
      w.gain_back[k] = back;
      for (const WayStep& st : w.steps)
        if (way_exit(st) && !st.revisit && st.node >= 0 && !now[static_cast<size_t>(st.node)] &&
            w.search.reached(st.node))
          more += 1000000;  // the way out
      w.gain[k] = more;
    }
  }
  // A car at rest that every way to an exit rides (base1's: the exit lies under its shaft, the
  // only links down are over its top as it goes down): its button first, then the exit.
  std::vector<int> needed;
  {
    reach_past(g, from, w.barred, w.node_door, f.bot.pilot.pilot.set.jumper, w.search, &w);
    std::vector<int32_t> exits;
    for (const WayStep& st : w.steps)
      if (way_exit(st) && !st.revisit && st.node >= 0 && w.search.reached(st.node))
        exits.push_back(st.node);
    std::vector<int32_t> with;
    for (size_t k = 0; k < w.cars.size() && !exits.empty(); k++) {
      if (dist(s.sv.edicts[static_cast<size_t>(w.cars[k])].origin, w.cars_at[k]) >= 1) continue;
      with = w.barred;
      with.push_back(w.cars[k]);
      reach_past(g, from, with, w.node_door, f.bot.pilot.pilot.set.jumper, w.search, &w);
      bool any = false;
      for (const int32_t x : exits) any = any || w.search.reached(x);
      if (!any) needed.push_back(w.cars[k]);
    }
    // The search (and the crossings shut) back to the doors as they are: a car's test left them
    // with the car shut, and base2's lift down to bunk1, its button on it, was never reached.
    if (!w.cars.empty() && !exits.empty())
      reach_past(g, from, w.barred, w.node_door, f.bot.pilot.pilot.set.jumper, w.search, &w);
    // A hatch shut over an exit (base2's first: a fall down a shaft under two turning leaves): its
    // button first, then the exit.
    for (const int h : w.hatches)
      for (size_t k = 0; k < w.turners.size(); k++)
        if (w.turners[k] == h &&
            dist(s.sv.edicts[static_cast<size_t>(h)].angles, w.turners_at[k]) < 1)
          needed.push_back(h);
  }
  const auto worth = [&](const WayStep& st) {
    if (st.kind == WayKind::key) return true;
    for (const int d : st.doors) {
      for (size_t k = 0; k < w.locked.size(); k++)
        if (w.locked[k] == d && w.gain[k] > 0) return true;
      for (const int c : needed)
        if (c == d) return true;
      // Or it uncovers a key not yet taken.
      for (const WayStep& kst : w.steps)
        if (kst.kind == WayKind::key && !kst.done)
          for (const int k : kst.covers)
            if (w.locked[static_cast<size_t>(k)] == d) return true;
    }
    return false;
  };
  reach_past(g, from, w.barred, w.node_door, f.bot.pilot.pilot.set.jumper, w.search, &w);
  // A place is gone to while he can come back from it, or go on from it to an exit: a one-way drop
  // into a pit to touch a trigger or look about held him there for the rest of the map (e1m6's
  // pit, from 13 s; e1m1's flooded secret). Only with nothing else to do is a one-way place gone
  // to (the drop that is the way on).
  reach_back(g, from, w.barred, w.node_door, f.bot.pilot.pilot.set.jumper, w);
  w.back_here.swap(w.returns);
  std::fill(w.back_exit.begin(), w.back_exit.end(), 0);
  w.back_exit.resize(g.nodes.size(), 0);
  for (const WayStep& st : w.steps)
    if (st.kind == WayKind::exit && st.node >= 0) {
      reach_back(g, st.node, w.barred, w.node_door, f.bot.pilot.pilot.set.jumper, w);
      for (size_t n = 0; n < g.nodes.size(); n++) w.back_exit[n] |= w.returns[n];
    }
  const auto round_trip = [&](int32_t n) {
    return n >= 0 && (w.back_here[static_cast<size_t>(n)] || w.back_exit[static_cast<size_t>(n)]);
  };
  bool strict_work = false;
  for (size_t n = 0; n < g.nodes.size() && !strict_work && !w.complete; n++)
    strict_work = !w.seen[n] && w.search.reached(static_cast<int32_t>(n)) && !no_place(n) &&
                  round_trip(static_cast<int32_t>(n));
  // A car's button (base1's: the car drops him to the exit) is a way out like the exit itself.
  const auto carries = [&](const WayStep& st) {
    if (st.kind != WayKind::button) return false;
    for (const int d : st.doors)
      for (const int c : w.cars)
        if (c == d) return true;
    return false;
  };
  for (const WayStep& st : w.steps)
    strict_work = strict_work || (!st.done && st.kind != WayKind::exit &&
                                  !(w.complete && carries(st)) && t >= st.left_until &&
                                  st.node >= 0 && w.search.reached(st.node) && round_trip(st.node));
  const auto reach = [&](int32_t n) {
    return w.search.reached(n) && (!strict_work || round_trip(n));
  };
  // A car's button to press before the exit it carries him to.
  bool car_first = false;
  for (const WayStep& st : w.steps)
    if (st.kind == WayKind::button && !st.done && t >= st.left_until && st.node >= 0 &&
        reach(st.node))
      for (const int d : st.doors)
        for (const int c : needed) car_first = car_first || c == d;

  bool unseen = false;
  for (size_t n = 0; n < g.nodes.size() && !unseen; n++)
    unseen = !w.seen[n] && reach(static_cast<int32_t>(n)) && !no_place(n);
  // (`complete`) He knows the map as a player who has learned it does: where every monster, secret
  // and button is, so no place has to be seen before the work or the exit (Niclas, 2026-11-21:
  // "it's learnable"). Places unseen are walked to only when nothing he knows of is left.
  const bool roam = unseen && !w.complete;
  // (`complete`) A step is open while not done once, not left and reached; the nearest monster
  // alive whose place is reached; the exit waits for all of them.
  for (WayStep& st : w.steps) st.ever = st.ever || st.done;
  // (A party: party.hpp) what a mate holds is not his while he has work of his own.
  const auto mate_has = [&](const WayStep& st) {
    const size_t k = static_cast<size_t>(&st - w.steps.data());
    return k < w.mates_steps.size() && w.mates_steps[k] != 0;
  };
  const auto mate_has_prey = [&](int e) {
    return static_cast<size_t>(e) < w.mates_prey.size() &&
           w.mates_prey[static_cast<size_t>(e)] != 0;
  };
  // 1 a mate's (his to help with once he has none of his own), 2 no one's (too far from the
  // people playing: never his).
  const auto held_step = [&](const WayStep& st) {
    const size_t k = static_cast<size_t>(&st - w.steps.data());
    return mate_has(st) && (!w.mates_help || w.mates_steps[k] > 1);
  };
  bool held_left = false, own_left = false;  // something open a mate holds; and of his own
  const auto open_step = [&](const WayStep& st) {
    if (st.kind == WayKind::exit || carries(st) || st.ever || t < st.left_until || st.node < 0 ||
        !reach(st.node))
      return false;
    if (!mate_has(st)) {
      own_left = true;
      return true;
    }
    held_left = true;
    return !held_step(st);
  };
  int prey = -1;
  std::vector<std::pair<int, int32_t>> preys;  // (`complete`) every monster reached: its node
  int32_t prey_node = -1;
  double prey_c = 0;
  bool left_to_do = false;
  if (w.complete && t < w.complete_until) {
    for (const WayStep& st : w.steps) left_to_do = open_step(st) || left_to_do;
    for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
      // A monster: Quake's by its flag, Quake II's (no such flag in its view) by its live box.
      const bool monster = (d.flags & sh::kFlagMonster) != 0 || d.solid == sh::kSolidSlidebox;
      if (d.free || !monster || d.health <= 0 || d.solid == sh::kSolidNot ||
          static_cast<size_t>(e) >= w.prey_left.size() || t < w.prey_left[static_cast<size_t>(e)])
        continue;
      // One only a blast puts down (a zombie) waits until he carries one: before that he stood
      // knocking e1m3's down again and again for the rest of the map.
      if (!blasts && f.arsenal->blast_only != nullptr && d.modelindex > 0 &&
          static_cast<size_t>(d.modelindex) < s.models.size() &&
          f.arsenal->blast_only(s.models[static_cast<size_t>(d.modelindex)]))
        continue;
      int32_t n = sb::nav_nearest(g, d.origin);
      if (n >= 0 && static_cast<size_t>(n) < nav.wet.size() && nav.wet[static_cast<size_t>(n)]) {
        // In the water (a fish): shot from the nearest dry place reached, not swum after.
        n = -1;
        float best_d = kWayDryShot;
        for (size_t k = 0; k < g.nodes.size(); k++) {
          if (nav.wet[k] || !reach(static_cast<int32_t>(k))) continue;
          const float dd = dist(g.nodes[k].origin, d.origin);
          if (dd < best_d) {
            best_d = dd;
            n = static_cast<int32_t>(k);
          }
        }
      }
      if (n < 0 || !reach(n)) continue;
      // One in a trap (a pool he cannot climb out of) waits, as a step there does, until he is in
      // it himself.
      const auto trap_at = [&](int32_t x) {
        return x >= 0 && static_cast<size_t>(x) < nav.trap.size() &&
               nav.trap[static_cast<size_t>(x)];
      };
      if (trap_at(n) && !trap_at(from)) continue;
      if (mate_has_prey(e)) {
        held_left = true;
        if (!w.mates_help || w.mates_prey[static_cast<size_t>(e)] > 1) continue;
      } else {
        own_left = true;
      }
      const double c = w.search.cost(n);
      preys.push_back({e, n});
      if (prey < 0 || c < prey_c) {
        prey = e;
        prey_node = n;
        prey_c = c;
      }
    }
    left_to_do = left_to_do || prey >= 0;
    // None of his own and some a mate holds: the exit waits, and the next plan helps with it.
    if (held_left) w.mates_skipped++;
    w.mates_help = !own_left && held_left;
    left_to_do = left_to_do || held_left;
    // Nothing reached for a moment (a door the last trigger opens still on its way) is not yet
    // the end: the exit waits kWayDone_s past the last thing left to do.
    if (left_to_do) w.busy_at = t;
    left_to_do = left_to_do || t - w.busy_at < kWayDone_s;
  }
  // The exit; else the nearest step not done; else the nearest place not seen.
  int pick = -1;
  double best = 0;
  // A lift he waits for that only its button calls (bunk1's floor that sinks into the pit
  // under it, back up after its wait): its nearest button pressed again, first.
  {
    const sb::Pilot& pp = f.bot.pilot.pilot;
    int32_t lift = -1;
    if (pp.mode == sb::PilotMode::wait && pp.wait_s > kWayCall_s && pp.k + 1 < pp.route.size())
      lift = g.nodes[static_cast<size_t>(pp.route[pp.k + 1])].mover;
    // Only one standing still at the top of its travel (a plat on its way down is waited for).
    const sb::NavMover* lm = nullptr;
    for (const sb::NavMover& mv : g.movers)
      if (mv.ent == lift && sb::ridden(mv) && !sb::moving_platform(mv)) lm = &mv;
    if (lm != nullptr) {
      const sh::Edict& le = s.sv.edicts[static_cast<size_t>(lift)];
      const float up = le.absmin.z - lm->absmin.z;
      if (up < lm->travel.z - 2 || up > lm->travel.z + 2 || le.velocity.z != 0) lm = nullptr;
    }
    if (lm == nullptr) lift = -1;
    for (size_t k = 0; lift >= 0 && k < w.steps.size(); k++) {
      const WayStep& st = w.steps[k];
      bool calls = false;
      for (const int d : st.doors) calls = calls || d == lift;
      if (st.kind != WayKind::button || !calls || way_exit(st) || st.node < 0 ||
          !w.search.reached(st.node))
        continue;
      const double c = w.search.cost(st.node);
      if (pick < 0 || c < best) {
        pick = static_cast<int>(k);
        best = c;
      }
    }
    if (pick >= 0) {
      w.steps[static_cast<size_t>(pick)].done = false;
      w.calling = pick;
    } else if (w.calling >= 0 && !w.steps[static_cast<size_t>(w.calling)].done &&
               t >= w.steps[static_cast<size_t>(w.calling)].left_until) {
      pick = w.calling;  // on his way to it
    } else {
      w.calling = -1;
    }
  }
  for (size_t k = 0; k < w.steps.size(); k++) {
    const WayStep& st = w.steps[k];
    // (A party beside people: party.hpp) the exit held for no one is the people's to take.
    if (left_to_do || car_first || !way_exit(st) || st.revisit || st.node < 0 || !reach(st.node) ||
        t < st.left_until || (w.complete && held_step(st)))
      continue;
    const double c = w.search.cost(st.node);
    if (pick < 0 || c < best) {
      pick = static_cast<int>(k);
      best = c;
    }
  }
  // What opens a door worth opening first; the rest only once nothing is left to explore.
  // (`complete`) Then, with nothing else to do, what opens the way back (a pit's hidden plat whose
  // trigger opens the wall at its top, shut again since he came down).
  const auto way_back = [&](const WayStep& st) {
    for (const int d : st.doors)
      for (size_t k = 0; k < w.locked.size(); k++)
        if (w.locked[k] == d && w.gain_back[k] > 0) return true;
    return false;
  };
  // A step in a trap (map_nav.hpp: a pool dropped into with no way back) is taken last, once
  // nothing else is left: base2's trigger in its south pool held him there for good, though the
  // lever the exit needed lay on the ledge above. So is a button that drains a pool.
  const auto trapped_in = [&](int32_t n) {
    return n >= 0 && static_cast<size_t>(n) < nav.trap.size() && nav.trap[static_cast<size_t>(n)];
  };
  const bool me_trapped = trapped_in(from);
  // A key's lock is walked to with a key in hand: none taken yet, it opens nothing.
  bool key_held = false;
  for (const WayStep& st : w.steps) key_held = key_held || (st.kind == WayKind::key && st.done);
  for (int pass = 0; pass < (w.complete ? 4 : 3) && pick < 0; pass++) {
    // Nothing left but the steps taken last: a way back to a map he has played comes first, before
    // any place not yet seen (he knows the map: the steps are all he needs to find).
    for (size_t k = 0; pass == 2 && k < w.steps.size(); k++) {
      const WayStep& st = w.steps[k];
      if (!st.revisit || st.node < 0 || !reach(st.node) || t < st.left_until) continue;
      const double c = w.search.cost(st.node);
      if (pick < 0 || c < best) {
        pick = static_cast<int>(k);
        best = c;
      }
    }
    if (pick >= 0) break;
    for (size_t k = 0; k < w.steps.size(); k++) {
      const WayStep& st = w.steps[k];
      if (way_exit(st) || st.done || t < st.left_until || st.node < 0 || !reach(st.node) ||
          (st.keyed && !key_held) || (left_to_do && carries(st)) || (w.complete && held_step(st)))
        continue;
      bool covered = false;
      for (const int c : st.covers)
        covered = covered || !locked_moved(w, s, static_cast<size_t>(c));
      if (covered) continue;
      const bool last = (!me_trapped && trapped_in(st.node)) || st.drains;
      if (pass == 3) {
        if (roam || prey >= 0 || !way_back(st)) continue;
      } else if (pass < 2 ? last : !last || roam) {
        continue;
      }
      if (pass == 0 ? !worth(st) : pass == 1 && roam) continue;
      if (pass == 1 && w.complete && st.ever) continue;  // a lift's button: pressed once is enough
      if (st.kind == WayKind::secret && !w.complete) continue;
      const double c = w.search.cost(st.node);
      if (pick < 0 || c < best) {
        pick = static_cast<int>(k);
        best = c;
      }
    }
  }
  // (`complete`) THE TOUR (tour.hpp): every step open and every monster reached, taken in the
  // order quickest through them all and out by the exit, not the nearest each time (which leaves a
  // straggler at the far end of the map for last). Steps taken last (in a trap, a pool's drain) and
  // the way back are left to the passes above.
  int toured = 0;  // 1 a step, 2 a monster, from the tour
  if (w.complete && w.toured && left_to_do && !me_trapped && w.out < 0) {
    const auto open_now = [&](int32_t stop) {
      // A monster in a trap (map_nav.hpp: a pool dropped into with no way back) is no stop of the
      // tour while he is out of it.
      if (stop < 0) {
        for (const auto& [e, n] : preys)
          if (e == -1 - stop) return !me_trapped && trapped_in(n) ? -1 : n;
        return -1;
      }
      const WayStep& st = w.steps[static_cast<size_t>(stop)];
      if (way_exit(st)) return st.node >= 0 && reach(st.node) ? st.node : -1;
      // A car's button is the way out (pressed on the way to the exit, after the rest): a tour
      // that took it with monsters left rode him out of base2 on his way back through it.
      if (st.done || st.ever || t < st.left_until || st.node < 0 || !reach(st.node) ||
          (st.keyed && !key_held) || st.drains || (!me_trapped && trapped_in(st.node)) ||
          carries(st) || held_step(st))
        return -1;
      for (const int c : st.covers)
        if (!locked_moved(w, s, static_cast<size_t>(c))) return -1;
      return st.node;
    };
    bool first_gone = w.tour.order.empty();
    if (!first_gone) {
      const int32_t s0 = w.tour_stops[static_cast<size_t>(w.tour.order.front() - 1)];
      first_gone = open_now(s0) < 0 || (s0 >= 0 && way_exit(w.steps[static_cast<size_t>(s0)]));
    }
    if (t >= w.tour_at || first_gone) {
      w.tour_at = t + kWayTour_s;
      w.tours++;
      // The stops: the nearest kWayTourMax open, then the nearest exit last.
      std::vector<std::pair<double, int32_t>> near;
      for (size_t k = 0; k < w.steps.size(); k++) {
        if (way_exit(w.steps[k])) continue;
        const int32_t n = open_now(static_cast<int32_t>(k));
        if (n >= 0) near.push_back({w.search.cost(n), static_cast<int32_t>(k)});
      }
      for (const auto& [e, n] : preys) near.push_back({w.search.cost(n), -1 - e});
      std::sort(near.begin(), near.end());
      if (near.size() > static_cast<size_t>(kWayTourMax)) near.resize(kWayTourMax);
      w.tour_stops.clear();
      w.tour_nodes.clear();
      for (const auto& [c, stop] : near) {
        w.tour_stops.push_back(stop);
        w.tour_nodes.push_back(open_now(stop));
      }
      int32_t out = -1;
      for (size_t k = 0; k < w.steps.size(); k++) {
        const WayStep& st = w.steps[k];
        if (!way_exit(st) || st.revisit || st.node < 0 || !reach(st.node) || t < st.left_until)
          continue;
        if (out < 0 ||
            w.search.cost(st.node) < w.search.cost(w.steps[static_cast<size_t>(out)].node))
          out = static_cast<int32_t>(k);
      }
      if (out >= 0) {
        w.tour_stops.push_back(out);
        w.tour_nodes.push_back(w.steps[static_cast<size_t>(out)].node);
      }
      const int32_t n = static_cast<int32_t>(w.tour_stops.size());
      const size_t row = static_cast<size_t>(n) + 1;
      w.tour_cost.assign(row * row, ge::ai::kUnreached);
      w.tour_cost[0] = 0;
      for (int32_t j = 0; j < n; j++)
        w.tour_cost[static_cast<size_t>(j) + 1] =
            w.search.cost(w.tour_nodes[static_cast<size_t>(j)]);
      for (int32_t i = 0; i < n; i++) {
        reach_past(g, w.tour_nodes[static_cast<size_t>(i)], w.barred, w.node_door,
                   f.bot.pilot.pilot.set.jumper, w.tour_search, &w);
        for (int32_t j = 0; j < n; j++) {
          const int32_t to = w.tour_nodes[static_cast<size_t>(j)];
          if (w.tour_search.reached(to))
            w.tour_cost[(static_cast<size_t>(i) + 1) * row + static_cast<size_t>(j) + 1] =
                w.tour_search.cost(to);
        }
      }
      tour_plan(w.tour, w.tour_cost, n, {}, out >= 0 ? n : 0);
    }
    for (const int32_t k : w.tour.order) {
      const int32_t stop = w.tour_stops[static_cast<size_t>(k - 1)];
      if (stop >= 0 && way_exit(w.steps[static_cast<size_t>(stop)])) break;
      const int32_t n = open_now(stop);
      if (n < 0) continue;
      if (stop >= 0) {
        pick = stop;
        toured = 1;
      } else {
        prey = -1 - stop;
        prey_node = n;
        prey_c = w.search.cost(n);
        toured = 2;
      }
      break;
    }
  }
  // (`complete`) The nearest monster alive when it is nearer than the step (or the tour's next).
  const bool chase =
      toured ? toured == 2
             : w.complete && prey >= 0 &&
                   (pick < 0 || way_exit(w.steps[static_cast<size_t>(pick)]) || prey_c < best);
  if (chase) pick = -1;
  // Its time in all, over every spell he chased it: one he keeps turning to between others (e2m6's
  // monster past the pool's bridges, the plat out of the water between) restarts the clock below
  // each time and was chased for the whole map.
  bool spent = false;
  if (chase && prey >= 0 && static_cast<size_t>(prey) < w.prey_spent.size()) {
    double& sp = w.prey_spent[static_cast<size_t>(prey)];
    if (w.prey_tick >= 0 && t - w.prey_tick < 1) sp += t - w.prey_tick;
    spent = sp > kWayPreySpent_s;
    if (spent) sp = 0;
  }
  w.prey_tick = chase ? t : -1;
  if (!spent && (!chase || prey != w.prey)) {
    w.prey = chase ? prey : -1;
    w.prey_since = t;
  } else if (spent || t - w.prey_since > kWayGiveUp_s) {
    w.prey = prey;
    // Gone for this long and still alive: left for a while, and for good after kWayTries.
    const size_t e = static_cast<size_t>(prey);
    w.prey_tries[e]++;
    w.prey_left[e] = w.prey_tries[e] >= kWayTries ? kWayForever : t + kWayRetry_s;
    w.stats.left++;
    if (w.prey_tries[e] >= kWayTries) w.stats.given_up++;
    w.prey = -1;
    w.plan_at = -1;
  }
  // (`complete`) STOOD STILL: no further than kWayStill in kWayStill_s with a step or a monster to
  // go to (a ledge his route cannot leave, a pack of monsters behind bars he keeps switching
  // between, each new one restarting its own clock): what he is on is left like any step, and
  // the tour is worked out again without it.
  // The clock runs on through a plan with nothing chosen (a fish out of reach of every node a
  // moment, then back): only moving off sets it again.
  if (w.complete && (w.still_since < 0 || dist(self, w.still_at) > kWayStill)) {
    w.still_at = self;
    w.still_since = t;
  } else if (w.complete &&
             (chase || (pick >= 0 && !way_exit(w.steps[static_cast<size_t>(pick)])))) {
    if (t - w.still_since > kWayStill_s) {
      w.still_since = t;
      w.stats.left++;
      w.stats.stood++;
      if (chase) {
        const size_t e = static_cast<size_t>(prey);
        w.prey_tries[e]++;
        w.prey_left[e] = w.prey_tries[e] >= kWayTries ? kWayForever : t + kWayRetry_s;
        if (w.prey_tries[e] >= kWayTries) w.stats.given_up++;
        w.prey = -1;
      } else {
        WayStep& st = w.steps[static_cast<size_t>(pick)];
        st.left_until = ++st.tries >= kWayTries ? kWayForever : t + kWayRetry_s;
        if (st.tries >= kWayTries) w.stats.given_up++;
        if (w.log) say_way(w, f, g, s, me, self, st.node, "stood still: left");
        pick = -1;
      }
      w.tour.order.clear();
      w.plan_at = -1;
    }
  }
  // (`complete`) A jump missed twice is never planned again by the pilot, but the way search
  // still counts on it: on e3m5 the one jump up to the top floor's far side, missed twice early,
  // left every place there a goal with no route and him standing still for minutes. With no
  // route to what he is on for kWayNoRoute_s, the jumps he gave up on are given another try.
  if (w.complete) {
    sb::Pilot& pp = f.bot.pilot.pilot;
    const bool no_route = pp.mode == sb::PilotMode::idle && pp.route.size() < 2;
    if (!no_route || pp.failed.empty()) {
      w.stuck_since = -1;
    } else if (w.stuck_since < 0) {
      w.stuck_since = t;
    } else if (t - w.stuck_since > kWayNoRoute_s) {
      pp.failed.clear();
      pp.short_jumps.clear();
      w.stats.forgiven++;
      w.stuck_since = -1;
    }
  }
  // An exit the pilot finds no way to (one his way search takes that his routes do not) is left
  // like any other step, but after kWayNoRoute_s with no route rather than kWayGiveUp_s: the walk
  // to an exit is often longer than that.
  if (pick >= 0 && pick == w.step && way_exit(w.steps[static_cast<size_t>(pick)])) {
    const sb::Pilot& pp = f.bot.pilot.pilot;
    const bool no_route = pp.mode == sb::PilotMode::idle && pp.route.size() < 2;
    if (!no_route) {
      w.no_route_since = -1;
    } else if (w.no_route_since < 0) {
      w.no_route_since = t;
    } else if (t - w.no_route_since > kWayNoRoute_s) {
      w.steps[static_cast<size_t>(pick)].left_until = t + kWayRetry_s;
      if (w.log) say_way(w, f, g, s, me, self, w.steps[static_cast<size_t>(pick)].node, "left");
      w.stats.left++;
      w.no_route_since = -1;
      pick = -1;
    }
  }
  // (`complete`) The lightning boss's buttons before anything else while he lives.
  if (w.complete && t < w.complete_until)
    if (const int b = way_bolt_pick(w, s); b >= 0) {
      pick = b;
      w.steps[static_cast<size_t>(b)].done = false;
    }
  if (pick != w.step) {
    w.step = pick;
    w.step_since = t;
    // A step is given kWayGiveUp_s past twice the walk to it: base3's key lock, 40 s across the
    // map, was left 25 s into the walk a few seconds short of it.
    const int32_t at = pick >= 0 ? w.steps[static_cast<size_t>(pick)].node : -1;
    w.step_for = kWayGiveUp_s + (at >= 0 && w.search.reached(at) ? 2 * w.search.cost(at) : 0);
  } else if (w.log && pick >= 0 && way_exit(w.steps[static_cast<size_t>(pick)]) &&
             t - w.step_since > kWayGiveUp_s && w.stats.plans % 40 == 0) {
    say_way(w, f, g, s, me, self, w.steps[static_cast<size_t>(pick)].node, "on the exit");
  } else if (pick >= 0 && !way_exit(w.steps[static_cast<size_t>(pick)]) &&
             t - w.step_since > w.step_for) {
    WayStep& st = w.steps[static_cast<size_t>(pick)];
    st.left_until = t + kWayRetry_s;  // left for a while
    if (w.complete && ++st.tries >= kWayTries) {
      st.left_until = kWayForever;  // and for good
      w.stats.given_up++;
    }
    if (w.log) say_way(w, f, g, s, me, self, st.node, "left");
    w.stats.left++;
    w.step = -1;
    w.plan_at = -1;
  }
  const int32_t was = f.objective;
  if (w.step >= 0) {
    const WayStep& st = w.steps[static_cast<size_t>(w.step)];
    w.kind = st.kind;
    f.objective = st.node;
  } else if (w.prey >= 0) {
    w.kind = WayKind::monster;
    f.objective = prey_node;
  } else {
    w.kind = WayKind::explore;
    // The nearest place not seen by the way there; one he spends kWayGiveUp_s on without
    // seeing it (and what lies about it) counts as seen.
    if (w.explore >= 0 && f.objective == w.explore && !w.seen[static_cast<size_t>(w.explore)] &&
        t - w.explore_since > kWayGiveUp_s) {
      const sh::Vec3f there = g.nodes[static_cast<size_t>(w.explore)].origin;
      if (w.log) say_way(w, f, g, s, me, self, w.explore, "given up");
      for (size_t n = 0; n < g.nodes.size(); n++)
        if (dist(g.nodes[n].origin, there) < kWaySeen) w.seen[n] = 1;
      w.stats.left++;
    }
    // He keeps to the place he chose while it is unseen and still reached: the nearest by the
    // way there changes as he moves (a river's swim nodes and its banks' floor nodes take turns),
    // and choosing again each plan swapped two places for minutes, the give-up's clock started
    // again at every swap so that neither was ever given up.
    int32_t far = -1;
    double far_c = 0;
    const bool keep = w.explore >= 0 && !w.seen[static_cast<size_t>(w.explore)] &&
                      reach(w.explore) && !no_place(static_cast<size_t>(w.explore));
    if (keep) far = w.explore;
    for (size_t n = 0; !keep && n < g.nodes.size(); n++) {
      if (w.seen[n] || !reach(static_cast<int32_t>(n))) continue;
      if (no_place(n)) continue;
      const double c = w.search.cost(static_cast<int32_t>(n));
      if (far < 0 || c < far_c) {
        far = static_cast<int32_t>(n);
        far_c = c;
      }
    }
    if (far != w.explore) {
      w.explore = far;
      w.explore_since = t;
    }
    f.objective = far;
    // Nothing left to do from where he stands for kWayIdle_s (out of a pool onto a ledge whose
    // doors only open from the other side): back into the water, where the swim field leads him
    // out to the map's main part.
    if (far >= 0 || w.idle_since < 0) w.idle_since = far >= 0 ? -1 : t;
    // (`complete`) Nothing at all to do for kWayIdle_s: what was left for a while is tried again
    // now rather than at its time (e1m1's lift button, pressed once, left while its car stood at
    // the top: the rest of the map lay below it, and he stood up there for a minute and a half).
    if (w.complete && far < 0 && w.idle_since >= 0 && t - w.idle_since > kWayIdle_s &&
        t >= w.hopeful_until + kWayIdle_s) {
      bool any = false;
      for (WayStep& st : w.steps)
        if (st.left_until > t && st.left_until < kWayForever) {
          st.left_until = -1;
          any = true;
        }
      for (double& until : w.prey_left)
        if (until > t && until < kWayForever) {
          until = -1;
          any = true;
        }
      for (WayStep& st : w.steps)  // and a lift's button, pressed once, again
        if (st.kind == WayKind::button && st.ever && !st.done) {
          st.ever = false;
          any = true;
        }
      // And for a while the doors are taken as open (a plat or a lift he rode that the plan holds
      // shut at rest, the way back down past it): his routes find out whether they are.
      // (The idle clock runs on: the swim back out of a pool below still comes at its time.)
      w.hopeful_until = t + kWayHopeful_s;
      w.plan_at = -1;
      w.stats.retried += any ? 1 : 0;
      w.stats.hopeful++;
    }
    // Out of the water on the far side: the swim back is over.
    if (w.out >= 0 && !w.trapped[static_cast<size_t>(from >= 0 ? from : 0)] &&
        s.world.entities[static_cast<size_t>(me)].waterlevel == 0) {
      w.out = w.dive = -1;
      w.idle_since = -1;
    }
    // Swimming with nothing reached (a pool whose way out his graph does not hold: `end`'s
    // start, chasing a fish into it): to the nearest bank there is, straight across the water.
    if (w.out < 0 && far < 0 && t - w.idle_since > kWayIdle_s &&
        s.world.entities[static_cast<size_t>(me)].waterlevel >= 2 && !nav.entries.empty()) {
      float best_d = 0;
      for (size_t k = 0; k < nav.entries.size(); k++) {
        const float d = dist(g.nodes[static_cast<size_t>(nav.entries[k].to)].origin, self);
        if (w.out < 0 || d < best_d) {
          w.out = static_cast<int>(k);
          best_d = d;
        }
      }
      w.dive = w.out;
      w.trapped.assign(g.nodes.size(), 0);
    }
    if (w.out < 0 && far < 0 && from >= 0 && t - w.idle_since > kWayIdle_s) {
      // The way in: the entry nearest by the way there; the way out: the entry on the far side
      // (one whose land he cannot walk to) nearest the way in.
      w.trapped.assign(g.nodes.size(), 0);
      for (size_t n = 0; n < g.nodes.size(); n++)
        w.trapped[n] = w.search.reached(static_cast<int32_t>(n)) ? 1 : 0;
      double best_c = 0;
      for (size_t k = 0; k < nav.entries.size(); k++) {
        const int32_t a = nav.entries[k].from;
        if (!w.search.reached(a)) continue;
        if (w.dive < 0 || w.search.cost(a) < best_c) {
          w.dive = static_cast<int>(k);
          best_c = w.search.cost(a);
        }
      }
      if (w.dive >= 0) {
        const sh::Vec3f in =
            g.nodes[static_cast<size_t>(nav.entries[static_cast<size_t>(w.dive)].to)].origin;
        float best_d = 0;
        for (size_t k = 0; k < nav.entries.size(); k++) {
          if (w.trapped[static_cast<size_t>(nav.entries[k].from)]) continue;
          const float d = dist(g.nodes[static_cast<size_t>(nav.entries[k].to)].origin, in);
          if (w.out < 0 || d < best_d) {
            w.out = static_cast<int>(k);
            best_d = d;
          }
        }
        if (w.out < 0) w.dive = -1;
      }
    }
    if (w.out >= 0) f.objective = nav.entries[static_cast<size_t>(w.dive)].from;
  }
  if (w.log && (f.objective != was || w.stats.plans % 40 == 0)) {
    char line[256];
    const sh::Vec3f at =
        f.objective >= 0 ? g.nodes[static_cast<size_t>(f.objective)].origin : sh::Vec3f{};
    std::snprintf(line, sizeof line,
                  "%7.1f s at %5.0f %5.0f %4.0f w%d: %s %s to %5.0f %5.0f %4.0f "
                  "(%zu shut)",
                  t, self.x, self.y, self.z,
                  static_cast<int>(s.world.entities[static_cast<size_t>(me)].waterlevel),
                  way_kind_name(w.kind),
                  w.step >= 0 ? w.steps[static_cast<size_t>(w.step)].name.c_str() : "", at.x, at.y,
                  at.z, w.barred.size());
    w.log->push_back(line);
  }
  w.stats.by_kind[static_cast<int>(w.kind)]++;
}

}  // namespace ge::shooterbots
