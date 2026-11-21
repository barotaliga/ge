// The way through a campaign map (way_through.hpp): the steps read once, the plan twice a second.
//
// ge:summary The way through a map: its exit, buttons, keys and triggers read; the plan
// ge:keywords way_init way_plan exit button key trigger explore barred door reach
// ge:skill shooter bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooters_edge/bots/way_through.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "progs/entity_text.hpp"
#include "shooter/map_spec.hpp"
#include "shooter/trace.hpp"
#include "shooter/usercmd.hpp"
#include "shooters_edge/bots/buttons.hpp"

namespace sb = ge::shooterbots;

namespace shooters_edge::bots {

namespace {

namespace pg = ge::progs;

constexpr int kKeySilver = 131072, kKeyGold = 262144;  // the keys' item bits
constexpr float kButtonClose = 320;  // at a button's spot this near, he presses it, units
constexpr double kPressFor_s = 4;    // and gives the press this long

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

// A player's box (origin at its middle, 16 across, 24 down and 32 up) touches the box.
bool touches(sh::Vec3f p, sh::Vec3f lo, sh::Vec3f hi) {
  return p.x + 16 >= lo.x && p.x - 16 <= hi.x && p.y + 16 >= lo.y && p.y - 16 <= hi.y &&
         p.z + 32 >= lo.z && p.z - 24 <= hi.z;
}

// The node a player standing on touches the box from, nearest its middle; else the nearest a step
// beyond it; else none.
int32_t node_in(const sb::NavGraph& g, sh::Vec3f lo, sh::Vec3f hi) {
  const sh::Vec3f mid = (lo + hi) * 0.5f;
  int32_t best = -1;
  float best_d = 0;
  for (size_t n = 0; n < g.nodes.size(); n++) {
    const sh::Vec3f o = g.nodes[n].origin;
    if (!touches(o, lo, hi)) continue;
    const float d = dist(o, mid);
    if (best < 0 || d < best_d) {
      best = static_cast<int32_t>(n);
      best_d = d;
    }
  }
  if (best >= 0) return best;
  // None inside (a thin trigger up in a doorway, one over a pit): the nearest in the box grown by
  // a player's size; none there, no node at all (a step no route leads to).
  const sh::Vec3f grow{32, 32, 56};
  for (size_t n = 0; n < g.nodes.size(); n++) {
    const sh::Vec3f o = g.nodes[n].origin;
    if (!touches(o, lo - grow, hi + grow)) continue;
    const float d = dist(o, mid);
    if (best < 0 || d < best_d) {
      best = static_cast<int32_t>(n);
      best_d = d;
    }
  }
  if (best >= 0) return best;
  const int32_t n = sb::nav_nearest(g, mid);
  return n >= 0 && dist(g.nodes[static_cast<size_t>(n)].origin, mid) < 256 ? n : -1;
}

// Every node a player at `from` gets to past no door in `barred` (nav_reach with the doors), on
// the links his `jumper` takes (no rocket jump without a launcher: base1's exit, reached by one
// down the lift's shaft, stood him still at the start with no route the pilot would take).
void reach_past(const sb::NavGraph& g, int32_t from, const std::vector<int32_t>& barred,
                const std::vector<int32_t>& node_door, const sb::NavJumper& jumper,
                ge::ai::Search& search) {
  search.reset(g.nodes.size());
  if (from < 0) return;
  search.seed(from);
  search.run_all([&](int32_t a) {
    const sb::NavNode& n = g.nodes[static_cast<size_t>(a)];
    for (int32_t k = 0; k < n.links; k++) {
      const sb::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      if (!jumper.takes(l)) continue;
      bool shut = false;
      const int32_t mv = g.nodes[static_cast<size_t>(l.to)].mover;
      const int32_t in = mv >= 0 ? mv : node_door[static_cast<size_t>(l.to)];  // or a bridge's
      for (const int32_t b : barred) shut |= l.mover == b || in == b;
      if (shut) continue;
      const double cost = search.cost(a) + l.time_s;
      if (search.improves(l.to, cost)) search.reach(l.to, cost, a);
    }
  });
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
  }
  return "?";
}

void way_init(WayThrough& w, const Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
              const ge::shooter::MapSpec& spec) {
  w.steps.clear();
  w.locked.clear();
  w.locked_at.clear();
  w.cars.clear();
  w.cars_at.clear();
  w.barred.clear();
  w.stats = WayStats{};
  w.plan_at = -1;
  w.step_since = 0;
  w.step = -1;
  w.explore = -1;
  w.explore_since = 0;
  w.idle_since = -1;
  w.dive = -1;
  w.out = -1;
  w.no_route_since = -1;
  w.kind = WayKind::explore;
  const sb::NavGraph& g = nav.graph;
  w.seen.assign(g.nodes.size(), 0);
  w.node_door.assign(g.nodes.size(), -1);
  w.barred_was.clear();
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  if (!pg::parse_entity_text(spec.entities.c_str(), blocks, error)) return;
  // What opens a door only something else opens: a name a shut door answers to, or a counter's
  // or a relay's whose own target does (two hops).
  const auto names_door = [&](const std::string& t, std::vector<int>& out) {
    bool any = false;
    for (const pg::EntityBlock& d : blocks) {
      const char* dc = pg::block_value(d, "classname");
      const char* dn = pg::block_value(d, "targetname");
      const char* dm = pg::block_value(d, "model");
      const char* sf = pg::block_value(d, "spawnflags");
      if (!dc || !dn || t != dn) continue;
      if ((std::strcmp(dc, "func_door") == 0 && !(sf && (std::atoi(sf) & 1) != 0)) ||
          std::strcmp(dc, "func_door_secret") == 0 || std::strcmp(dc, "func_plat") == 0) {
        any = true;
        const int e = dm ? edict_of(s, dm) : -1;
        if (e >= 0) out.push_back(e);
      }
    }
    return any;
  };
  const auto opens = [&](const char* t, std::vector<int>& out) {
    if (!t) return false;
    bool any = names_door(t, out);
    for (const pg::EntityBlock& d : blocks) {
      const char* dc = pg::block_value(d, "classname");
      const char* dn = pg::block_value(d, "targetname");
      const char* dt = pg::block_value(d, "target");
      if (!dc || !dn || !dt || std::strcmp(dn, t) != 0) continue;
      if (std::strcmp(dc, "trigger_counter") == 0 || std::strcmp(dc, "trigger_relay") == 0 ||
          std::strcmp(dc, "trigger_key") == 0)
        any = names_door(dt, out) || any;
    }
    return any;
  };
  // A name answered by a key's lock (Quake II's `trigger_key`), and the map a name's level
  // change leads to (Quake II's exits: a trigger whose target is a `target_changelevel`).
  const auto keyed = [&](const char* t) {
    for (const pg::EntityBlock& d : blocks) {
      const char* dc = pg::block_value(d, "classname");
      const char* dn = pg::block_value(d, "targetname");
      if (t && dc && dn && std::strcmp(dn, t) == 0 && std::strcmp(dc, "trigger_key") == 0)
        return true;
    }
    return false;
  };
  const auto level_change = [&](const char* t) -> const char* {
    for (const pg::EntityBlock& d : blocks) {
      const char* dc = pg::block_value(d, "classname");
      const char* dn = pg::block_value(d, "targetname");
      if (t && dc && dn && std::strcmp(dn, t) == 0 && std::strcmp(dc, "target_changelevel") == 0) {
        const char* map = pg::block_value(d, "map");
        return map ? map : "";
      }
    }
    return nullptr;
  };
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* model = pg::block_value(b, "model");
    const char* target = pg::block_value(b, "target");
    const char* name = pg::block_value(b, "targetname");
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
      const char* map = pg::block_value(b, "map");
      st.name = map ? map : "";
    } else if (const char* to = trigger && brush && target ? level_change(target) : nullptr) {
      st.kind = WayKind::exit;  // Quake II's: walked through, it fires the level change
      st.name = to;
      // The way back to the map he came from (base2's start: the car up to base1) leads on to
      // nothing: neither a goal nor a reason to press the button that reaches it.
      std::string_view goes(to);
      if (!goes.empty() && goes[0] == '*') goes.remove_prefix(1);
      goes = goes.substr(0, goes.find('$'));
      if (!w.back.empty() && goes == w.back) continue;
    } else if ((std::strcmp(cn, "trigger_once") == 0 || std::strcmp(cn, "trigger_multiple") == 0) &&
               brush && target && !name) {
      st.kind = WayKind::trigger;  // one fired by something else is not walked into
      st.target = target;
      st.opens = opens(target, st.doors);
      st.keyed = keyed(target);
    } else if (std::strcmp(cn, "item_key1") == 0 || std::strcmp(cn, "item_key2") == 0) {
      const char* org = pg::block_value(b, "origin");
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
    } else if (std::strncmp(cn, "key_", 4) == 0) {
      // Quake II's keys: taken when the thing lying there is gone.
      const char* org = pg::block_value(b, "origin");
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
    } else if (std::strcmp(cn, "func_door") == 0 && name && brush) {
      const char* sf = pg::block_value(b, "spawnflags");
      if (std::strcmp(cn, "func_door") == 0 && sf && (std::atoi(sf) & 1) != 0) {
        // Starts open: a bridge still in is shut to him (its nodes are where it slides out to);
        // any other is open.
        const int e = edict_of(s, model);
        for (const NavBridge& br : nav.bridges)
          if (br.ent == e && name) {
            w.locked.push_back(e);
            w.locked_at.push_back(br.spawn);
            for (const int32_t n : br.nodes) w.node_door[static_cast<size_t>(n)] = e;
          }
        continue;
      }
      const int e = edict_of(s, model);
      if (e < 0) continue;
      // A door whose own button rides inside it is a lift's car, not a barrier (base1's exit and
      // base2's start: a hollow car the button on its inner wall sends along its shaft): it keeps
      // no one out, and barring it kept him from the button that moves it.
      bool holds = false, bars_one = true;
      for (const sb::NavNode& n : g.nodes) holds = holds || n.mover == e;
      const sh::Edict& de = s.sv.edicts[static_cast<size_t>(e)];
      for (const ButtonSpot& bs : f.button_spots) {
        bool its = false;
        for (const int d : bs.doors) its = its || d == e;
        constexpr float kIn = 16;  // the button's middle this near the car's box counts as in it
        if (its && bs.at.x > de.absmin.x - kIn && bs.at.x < de.absmax.x + kIn &&
            bs.at.y > de.absmin.y - kIn && bs.at.y < de.absmax.y + kIn &&
            bs.at.z > de.absmin.z - kIn && bs.at.z < de.absmax.z + kIn)
          bars_one = false;
      }
      if (holds && !bars_one) {
        // Still watched: the button that sends it is pressed only once it has moved.
        w.cars.push_back(e);
        w.cars_at.push_back(s.sv.edicts[static_cast<size_t>(e)].origin);
        continue;
      }
      w.locked.push_back(e);
      w.locked_at.push_back(s.sv.edicts[static_cast<size_t>(e)].origin);
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
    if (st.name.empty()) st.name = cn;
    w.steps.push_back(st);
  }
  for (size_t k = 0; k < f.button_spots.size(); k++) {
    const ButtonSpot& bs = f.button_spots[k];
    WayStep st;
    st.kind = WayKind::button;
    st.button = static_cast<int>(k);
    st.lo = st.hi = s.sv.edicts[static_cast<size_t>(bs.ent)].origin;  // where it rests
    st.node = bs.node;
    st.name = bs.shoot ? "shot button" : "button";
    const sh::Edict& be = s.sv.edicts[static_cast<size_t>(bs.ent)];
    for (const pg::EntityBlock& b : blocks) {
      const char* model = pg::block_value(b, "model");
      if (model && be.modelindex > 0 && static_cast<size_t>(be.modelindex) < s.models.size() &&
          s.models[static_cast<size_t>(be.modelindex)] == model)
        st.opens = opens(pg::block_value(b, "target"), st.doors);
    }
    // Arrived from another map in a car (base2's start: the lift down from base1), the car's
    // button sends him back up to the way back: no step of his.
    bool car_back = false;
    if (!w.back.empty()) {
      const sh::Vec3f at = s.sv.edicts[1].origin;
      for (const int d : st.doors) {
        const sh::Edict& de = s.sv.edicts[static_cast<size_t>(d)];
        car_back = car_back || (at.x > de.absmin.x && at.x < de.absmax.x && at.y > de.absmin.y &&
                                at.y < de.absmax.y && at.z > de.absmin.z && at.z < de.absmax.z);
      }
    }
    if (!car_back) w.steps.push_back(st);
  }
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
  w.plan_at = t;
  w.stats.plans++;
  // Where he stands now is explored.
  for (size_t n = 0; n < g.nodes.size(); n++)
    if (!w.seen[n] && dist(g.nodes[n].origin, self) < kWaySeen) w.seen[n] = 1;

  // What is done.
  const int items = read_stats(s, me, *f.arsenal).items;
  const auto door_open = [&](int ent) {
    for (size_t k = 0; k < w.locked.size(); k++)
      if (w.locked[k] == ent)
        return dist(s.sv.edicts[static_cast<size_t>(ent)].origin, w.locked_at[k]) >= 1;
    for (size_t k = 0; k < w.cars.size(); k++)
      if (w.cars[k] == ent)
        return dist(s.sv.edicts[static_cast<size_t>(ent)].origin, w.cars_at[k]) >= 1;
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
    if (st.done) continue;
    switch (st.kind) {
      case WayKind::key:
        if (st.ent >= 0) {
          const sh::Edict& k = s.sv.edicts[static_cast<size_t>(st.ent)];
          st.done = k.free || k.modelindex <= 0;
        } else {
          st.done = (items & st.key_bit) != 0;
        }
        break;
      case WayKind::trigger:
        if (st.keyed && !st.doors.empty()) {  // walked through with the key held: its doors open
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
    if (st.done) w.stats.steps_done++;
  }
  // The doors shut now that only something else opens.
  w.barred.clear();
  for (size_t k = 0; k < w.locked.size(); k++)
    if (dist(s.sv.edicts[static_cast<size_t>(w.locked[k])].origin, w.locked_at[k]) < 1)
      w.barred.push_back(w.locked[k]);
  f.bot.pilot.pilot.shut = w.barred;  // his own routes never through them either
  f.bot.pilot.pilot.hollow = w.cars;  // and a car at rest is walked through
  if (w.barred != w.barred_was) {     // nor onto a bridge still in
    w.barred_was = w.barred;
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
  }
  const int32_t from = sb::nav_nearest(g, self);
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
  {
    reach_past(g, from, w.barred, w.node_door, f.bot.pilot.pilot.set.jumper, w.search);
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
      reach_past(g, from, others, w.node_door, f.bot.pilot.pilot.set.jumper, w.search);
      int64_t more = 0;
      for (size_t n = 0; n < g.nodes.size(); n++)
        if (!now[n] && !w.seen[n] && w.search.reached(static_cast<int32_t>(n)) && !no_place(n))
          more++;
      for (const WayStep& st : w.steps)
        if (st.kind == WayKind::exit && st.node >= 0 && !now[static_cast<size_t>(st.node)] &&
            w.search.reached(st.node))
          more += 1000000;  // the way out
      w.gain[k] = more;
    }
  }
  // A car at rest that every way to an exit rides (base1's: the exit lies under its shaft, the
  // only links down are over its top as it goes down): its button first, then the exit.
  std::vector<int> needed;
  {
    reach_past(g, from, w.barred, w.node_door, f.bot.pilot.pilot.set.jumper, w.search);
    std::vector<int32_t> exits;
    for (const WayStep& st : w.steps)
      if (st.kind == WayKind::exit && st.node >= 0 && w.search.reached(st.node))
        exits.push_back(st.node);
    std::vector<int32_t> with;
    for (size_t k = 0; k < w.cars.size() && !exits.empty(); k++) {
      if (dist(s.sv.edicts[static_cast<size_t>(w.cars[k])].origin, w.cars_at[k]) >= 1) continue;
      with = w.barred;
      with.push_back(w.cars[k]);
      reach_past(g, from, with, w.node_door, f.bot.pilot.pilot.set.jumper, w.search);
      bool any = false;
      for (const int32_t x : exits) any = any || w.search.reached(x);
      if (!any) needed.push_back(w.cars[k]);
    }
  }
  const auto worth = [&](const WayStep& st) {
    if (st.kind == WayKind::key) return true;
    for (const int d : st.doors) {
      for (size_t k = 0; k < w.locked.size(); k++)
        if (w.locked[k] == d && w.gain[k] > 0) return true;
      for (const int c : needed)
        if (c == d) return true;
    }
    return false;
  };
  reach_past(g, from, w.barred, w.node_door, f.bot.pilot.pilot.set.jumper, w.search);
  // A car's button to press before the exit it carries him to.
  bool car_first = false;
  for (const WayStep& st : w.steps)
    if (st.kind == WayKind::button && !st.done && t >= st.left_until && st.node >= 0 &&
        w.search.reached(st.node))
      for (const int d : st.doors)
        for (const int c : needed) car_first = car_first || c == d;

  // The exit; else the nearest step not done; else the nearest place not seen.
  int pick = -1;
  double best = 0;
  for (size_t k = 0; k < w.steps.size(); k++) {
    const WayStep& st = w.steps[k];
    if (car_first || st.kind != WayKind::exit || st.node < 0 || !w.search.reached(st.node) ||
        t < st.left_until)
      continue;
    const double c = w.search.cost(st.node);
    if (pick < 0 || c < best) {
      pick = static_cast<int>(k);
      best = c;
    }
  }
  // What opens a door worth opening first; the rest only once nothing is left to explore.
  bool unseen = false;
  for (size_t n = 0; n < g.nodes.size() && !unseen; n++)
    unseen = !w.seen[n] && w.search.reached(static_cast<int32_t>(n)) && !no_place(n);
  for (int pass = 0; pass < 2 && pick < 0; pass++)
    for (size_t k = 0; k < w.steps.size(); k++) {
      const WayStep& st = w.steps[k];
      if (st.kind == WayKind::exit || st.done || t < st.left_until || st.node < 0 ||
          !w.search.reached(st.node))
        continue;
      if (pass == 0 ? !worth(st) : unseen) continue;
      const double c = w.search.cost(st.node);
      if (pick < 0 || c < best) {
        pick = static_cast<int>(k);
        best = c;
      }
    }
  // An exit the pilot finds no way to (one his way search takes that his routes do not) is left
  // like any other step, but after kWayNoRoute_s with no route rather than kWayGiveUp_s: the walk
  // to an exit is often longer than that.
  if (pick >= 0 && pick == w.step && w.steps[static_cast<size_t>(pick)].kind == WayKind::exit) {
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
  if (pick != w.step) {
    w.step = pick;
    w.step_since = t;
  } else if (w.log && pick >= 0 && w.steps[static_cast<size_t>(pick)].kind == WayKind::exit &&
             t - w.step_since > kWayGiveUp_s && w.stats.plans % 40 == 0) {
    say_way(w, f, g, s, me, self, w.steps[static_cast<size_t>(pick)].node, "on the exit");
  } else if (pick >= 0 && w.steps[static_cast<size_t>(pick)].kind != WayKind::exit &&
             t - w.step_since > kWayGiveUp_s) {
    w.steps[static_cast<size_t>(pick)].left_until = t + kWayRetry_s;  // left for a while
    if (w.log) say_way(w, f, g, s, me, self, w.steps[static_cast<size_t>(pick)].node, "left");
    w.stats.left++;
    w.step = -1;
    w.plan_at = -1;
  }
  const int32_t was = f.objective;
  if (w.step >= 0) {
    const WayStep& st = w.steps[static_cast<size_t>(w.step)];
    w.kind = st.kind;
    f.objective = st.node;
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
                      w.search.reached(w.explore) && !no_place(static_cast<size_t>(w.explore));
    if (keep) far = w.explore;
    for (size_t n = 0; !keep && n < g.nodes.size(); n++) {
      if (w.seen[n] || !w.search.reached(static_cast<int32_t>(n))) continue;
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
    // Out of the water on the far side: the swim back is over.
    if (w.out >= 0 && !w.trapped[static_cast<size_t>(from >= 0 ? from : 0)] &&
        s.world.entities[static_cast<size_t>(me)].waterlevel == 0) {
      w.out = w.dive = -1;
      w.idle_since = -1;
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

namespace {

// A body that blocks as a box (a monster, a barrel; not a player) alive against him: its box within
// kWayAgainst of his.
constexpr float kWayAgainst = 8;
bool body_against(const ge::shooterbots::BotView& s, int me) {
  const sh::Edict& e = s.sv.edicts[static_cast<size_t>(me)];
  for (int k = s.sv.set.maxclients + 1; k < s.sv.num_edicts; k++) {
    const sh::Edict& o = s.sv.edicts[static_cast<size_t>(k)];
    if (o.free || o.health <= 0 || (o.solid != sh::kSolidBbox && o.solid != sh::kSolidSlidebox))
      continue;
    const sh::Vec3f lo = o.origin + o.mins, hi = o.origin + o.maxs;
    const sh::Vec3f mlo = e.origin + e.mins, mhi = e.origin + e.maxs;
    if (lo.x < mhi.x + kWayAgainst && hi.x > mlo.x - kWayAgainst && lo.y < mhi.y + kWayAgainst &&
        hi.y > mlo.y - kWayAgainst && lo.z < mhi.z + kWayAgainst && hi.z > mlo.z - kWayAgainst)
      return true;
  }
  return false;
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
  if (!body_against(s, me) && static_cast<int>((t - w.still_since) * 4) % 4 == 0)
    cmd.buttons |= sh::kButtonJump;
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
  // Over the lip of a pit he could not climb back out of: the keys let go, and a run into it
  // braked by the wish turned back along it.
  cmd.forward = static_cast<int16_t>(-cmd.forward);
  cmd.side = static_cast<int16_t>(-cmd.side);
  cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonJump);
  return true;
}

}  // namespace shooters_edge::bots
