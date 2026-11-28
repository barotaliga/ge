// The way through's parts (way_parts.hpp): the small reads way_init and way_plan share.
//
// ge:summary The way through's shared parts: boxes, edicts, open links, the searches past bars
// ge:keywords way_parts touches crosses link_open reach_past reach_back
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp tests/shooterbots_way_parts_test.cpp
#include "shooterbots/way_parts.hpp"

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

namespace ge::shooterbots::way_parts {

double way_idle(double since, bool busy, double t) { return busy ? -1 : since < 0 ? t : since; }

bool way_forgive(WayStep& st, const NavGraph& g, int32_t from) {
  const auto in = [&](int32_t n) { return n >= 0 && static_cast<size_t>(n) < g.nodes.size(); };
  if (st.forgiven || st.left_until < kWayForever || !in(st.given_from) || !in(from)) return false;
  if (dist(g.nodes[static_cast<size_t>(from)].origin,
           g.nodes[static_cast<size_t>(st.given_from)].origin) < kWayGivenFar)
    return false;
  st.forgiven = true;
  st.left_until = -1;
  st.tries = kWayTries - 1;
  return true;
}

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

// Where a brush entity's model stood when the map spawned it (its `origin`, else the map's own):
// a level entered again keeps its doors and buttons where they were left.
sh::Vec3f spawned_at(const sb::EntityBlock& b) {
  const char* o = sb::block_value(b, "origin");
  return o ? origin_of(o) : sh::Vec3f{};
}

// Whether locked door `k` has moved off where it stood shut (a turning bridge: turned; a bridge
// slid out of the floor, `kLockedOut`: all the way out to where the map drew it, `locked_at`, so
// one still rising or sinking again, as e2m6's pool bridges do on their clock, is still shut), or
// is gone.
bool locked_moved(const WayThrough& w, const ge::shooterbots::BotView& s, size_t k) {
  const sh::Edict& d = s.sv.edicts[static_cast<size_t>(w.locked[k])];
  const uint8_t kind = k < w.locked_turns.size() ? w.locked_turns[k] : 0;
  if (d.free) return true;  // gone: a field turned off, a wall blown away
  if (kind == kLockedOut) return dist(d.origin, w.locked_at[k]) < 1;
  if (kind == kLockedGone) return d.solid != sh::kSolidBsp;
  if (kind == kLockedLit) return d.solid == sh::kSolidBsp;
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
  for (const int32_t li : w.closed_shut) w.link_shut[static_cast<size_t>(li)] = 1;
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
                ge::ai::Search& search, WayThrough* w) {
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
        // The mover named by its edict, as the node's is (`m`).
        line += std::string(sb::nav_kind_name(l.kind)) +
                (l.mover >= 0 ? "/mover " + std::to_string(l.mover) + " " : " ");
        break;
      }
  }
  w.log->push_back(line);
}

}  // namespace ge::shooterbots::way_parts
