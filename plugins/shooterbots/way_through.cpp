// The way through a campaign map (way_through.hpp): the steps read once, the plan twice a second.
//
// ge:summary The way through a map: its exit, buttons, keys and triggers read; the plan
// ge:keywords way_init way_plan exit button key trigger explore barred door reach
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooterbots/way_through.hpp"
#include "shooterbots/way_bosses.hpp"
#include "shooterbots/way_exits.hpp"
#include "shooterbots/way_gates.hpp"
#include "shooterbots/way_keys.hpp"
#include "shooterbots/way_parts.hpp"
#include "shooterbots/way_shots.hpp"

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

constexpr float kGateWide = 64;      // an explosive narrower both ways is no way's gate, units
constexpr float kButtonClose = 320;  // at a button's spot this near, he presses it, units
constexpr double kPressFor_s = 4;    // and gives the press this long
constexpr float kWayShotFrom = 600;  // (`complete`) a secret wall is shot from this near, units
constexpr float kWayDryShot = 700;   // (`complete`) a monster in the water is shot from this near
constexpr double kWayRide_s = 12;    // the longest ride a plan holds through, s
constexpr float kWayAfloat = 768;    // how far the plan looks for a node to start from in the air

}  // namespace

using namespace way_parts;

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
    case WayKind::arm:
      return "arm";
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
  w.arm_ent = -1;
  w.arm_tick = 0;
  w.arm_spent.assign(s.sv.edicts.size(), 0);
  w.arm_left.assign(s.sv.edicts.size(), 0);
  w.prey_tick = -1;
  const sb::NavGraph& g = nav.graph;
  w.seen.assign(g.nodes.size(), 0);
  w.node_door.assign(g.nodes.size(), -1);
  w.barred_was.clear();
  w.crossings.clear();
  w.walls.clear();
  w.closers.clear();
  w.closed_shut.clear();
  w.closed_from.clear();
  w.closed_was.clear();
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
  // The edict of a gate (`gates` below) at the start: a named wall, explosive or waiting train
  // standing in the world; -1 for anything else. One out of the world at the start bars nothing:
  // a wall or an explosive that appears when fired (spawnflag 1), a toggled wall not on (a wall's
  // 1 or 2 without 4). A small explosive (a crate, a lamp: base3's four that a button breaks) is
  // walked round, no gate.
  const auto gate_of = [&](const sb::EntityBlock& d) -> int {
    const char* dc = sb::block_value(d, "classname");
    const char* dm = sb::block_value(d, "model");
    const char* dsf = sb::block_value(d, "spawnflags");
    const int flags = dsf ? std::atoi(dsf) : 0;
    if (!dc || !dm || dm[0] != '*' || !sb::block_value(d, "targetname")) return -1;
    const bool wall = std::strcmp(dc, "func_wall") == 0;
    const bool blast = std::strcmp(dc, "func_explosive") == 0;
    const bool train = std::strcmp(dc, "func_train") == 0 && (flags & 1) == 0;
    if (!wall && !blast && !train) return -1;
    if (wall ? (flags & 3) != 0 && (flags & 4) == 0 : blast && (flags & 1) != 0) return -1;
    const int dmi = std::atoi(dm + 1);
    const size_t models = !spec.models.empty() ? spec.models.size()
                          : spec.brushes       ? spec.brushes->models.size()
                                               : 0;
    if (dmi <= 0 || static_cast<size_t>(dmi) >= models) return -1;
    sh::Vec3f glo, ghi;
    if (!spec.models.empty()) {
      glo = spec.models[static_cast<size_t>(dmi)].mins;
      ghi = spec.models[static_cast<size_t>(dmi)].maxs;
    } else {
      const ge::brush::Model& gm = spec.brushes->models[static_cast<size_t>(dmi)];
      glo = {gm.mins[0], gm.mins[1], gm.mins[2]};
      ghi = {gm.maxs[0], gm.maxs[1], gm.maxs[2]};
    }
    if (blast && std::fmax(ghi.x - glo.x, ghi.y - glo.y) < kGateWide) return -1;
    const int e = edict_of(s, dm);
    return e >= 0 && !s.sv.edicts[static_cast<size_t>(e)].free ? e : -1;
  };
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
      if (const int gate = gate_of(d); gate >= 0) {
        any = true;  // a gate (a field, rubble, a waiting train): open once gone or moved
        out.push_back(gate);
      } else if ((std::strcmp(dc, "func_door") == 0 && !(sf && (std::atoi(sf) & 1) != 0)) ||
                 std::strcmp(dc, "func_door_secret") == 0 || std::strcmp(dc, "func_plat") == 0 ||
                 std::strcmp(dc, "func_plat2") == 0 || std::strcmp(dc, "func_togglewall") == 0) {
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
      const char *dt = sb::block_value(d, "target"), *dk = sb::block_value(d, "killtarget");
      if (!dc || !dn || (!dt && !dk) || std::strcmp(dn, t) != 0) continue;
      // (A door that fires its own target passes it on: base2's lever opens the exit's door.)
      const auto is = [&](const char* c) { return std::strcmp(dc, c) == 0; };
      const bool relays = is("trigger_counter") || is("trigger_relay") || is("trigger_key");
      if (dt && (relays || is("func_door_rotating") || is("func_door")))
        any = names_door(dt, out) || any;
      // What a counter or a relay removes opens too (boss1's wall before the lift to the boss).
      if (dk && relays) any = names_door(dk, out) || any;
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
  // A name nothing fires (e3m5's trigger hung in the pit, named but never a target): touched,
  // like one with no name.
  const auto fired = [&](const char* n) {
    if (!n) return false;
    for (const sb::EntityBlock& d : blocks)
      for (const char* key : {"target", "killtarget"}) {
        const char* v = sb::block_value(d, key);
        if (v && std::strcmp(v, n) == 0) return true;
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
  // The level change a trigger's target reaches through relays and key locks (rboss's last exit:
  // a trigger, a `trigger_key` wanting the nuke, a relay, then the end), up to four hops.
  const auto level_change_via = [&](const char* t) -> const char* {
    for (int hop = 0; t != nullptr && hop < 4; hop++) {
      if (const char* to = level_change(t)) return to;
      const char* next = nullptr;
      for (const sb::EntityBlock& d : blocks) {
        const char* dc = sb::block_value(d, "classname");
        const char* dn = sb::block_value(d, "targetname");
        if (dc && dn && std::strcmp(dn, t) == 0 &&
            (std::strcmp(dc, "trigger_relay") == 0 || std::strcmp(dc, "trigger_key") == 0))
          next = sb::block_value(d, "target");
      }
      t = next;
    }
    return nullptr;
  };
  const bool quake_keys = way_quake_keys(blocks);  // keys are items: their doors open to a touch
  w.key_doors.clear();
  std::vector<int> gates;  // the shut doors barred by the links through their shut boxes
  way_key_hatches(w, s, blocks, gates);
  // The links that pass through secret wall `e` (shut while it is barred).
  // One lying flat (e2m1's slab over the pool's shaft) is crossed through its floor.
  constexpr float kOnSlab = 64;  // a ride's foot this far over a flat wall stands on it, units
  const auto add_crossings = [&](int e) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    const sh::Vec3f size = d.absmax - d.absmin;
    const bool flat = size.z < size.x && size.z < size.y;
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sb::NavNode& n = g.nodes[a];
      for (int32_t k = 0; k < n.links; k++) {
        const int32_t li = n.first_link + k;
        const sb::NavLink& l = g.links[static_cast<size_t>(li)];
        const sh::Vec3f to = g.nodes[static_cast<size_t>(l.to)].origin;
        // A ride boarded on it: the plat's trigger lies under the slab (e3m1's plat *64 at the
        // foot of its shaft, under slab t58), so it never rises while the slab is there.
        // (Its box is a unit wider all round.)
        const bool on = l.kind == sb::NavKind::ride && n.origin.z >= d.absmax.z - 4 &&
                        n.origin.z - d.absmax.z <= kOnSlab && n.origin.x >= d.absmin.x &&
                        n.origin.x <= d.absmax.x && n.origin.y >= d.absmin.y &&
                        n.origin.y <= d.absmax.y;
        if (flat ? on || way_through_floor(n.origin, to, d.absmin, d.absmax)
                 : crosses(n.origin, to, d.absmin, d.absmax))
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
    if (std::strcmp(cn, "func_door") == 0 && brush)
      way_note_closer(w, s, edict_of(s, model), sb::block_value(b, "spawnflags"),
                      name != nullptr && name[0] != 0);
    WayStep st;
    const bool trigger =
        std::strcmp(cn, "trigger_once") == 0 || std::strcmp(cn, "trigger_multiple") == 0;
    if (std::strcmp(cn, "trigger_changelevel") == 0 && brush) {
      st.kind = WayKind::exit;
      const char* map = sb::block_value(b, "map");
      st.name = map ? map : "";
      if (avoided(st.name)) continue;  // a secret level the run leaves for its own
      // One the mode never spawns is no exit (hip2m2's deathmatch way out beside the start, the
      // campaign's down through the floor: he stood at the first for minutes). A trigger keeps no
      // model once spawned, so it is known by its box.
      if (!spec.models.empty()) {
        const sh::Vec3f lo = spec.models[static_cast<size_t>(m)].mins,
                        hi = spec.models[static_cast<size_t>(m)].maxs;
        bool live = false;
        for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts && !live; e++) {
          const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
          live = !d.free && std::fabs(d.absmin.x - lo.x) < 2 && std::fabs(d.absmin.y - lo.y) < 2 &&
                 std::fabs(d.absmin.z - lo.z) < 2 && std::fabs(d.absmax.x - hi.x) < 2 &&
                 std::fabs(d.absmax.y - hi.y) < 2 && std::fabs(d.absmax.z - hi.z) < 2;
        }
        if (!live) continue;
      }
    } else if (w.complete && std::strcmp(cn, "trigger_secret") == 0 && brush) {
      st.kind = WayKind::secret;  // Quake's: walked into, it counts
      st.name = cn;
    } else if (w.complete && trigger && brush && target && !name && fires_secret(target)) {
      st.kind = WayKind::secret;
      st.name = "secret";
    } else if (const char* to = trigger && brush && target ? level_change_via(target) : nullptr) {
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
               brush && target && (!fired(name) || trigger_waits(b)) && !way_shot_only(b)) {
      st.kind = WayKind::trigger;  // one fired by something else is not walked into, but one
      st.target = target;          // made live by it is, once it is
      st.repeats = std::strcmp(cn, "trigger_multiple") == 0;
      if (fired(name)) way_gate_names(st, name, blocks, s);
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
      st.node = way_node_in(g, st.lo, st.hi);
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
      st.node = way_node_in(g, {o.x - 16, o.y - 16, o.z}, {o.x + 16, o.y + 16, o.z + 32});
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
      st.node = way_node_in(g, st.lo, st.hi);
      st.name = cn;
      w.steps.push_back(st);
      continue;
    } else if ((std::strcmp(cn, "func_plat") == 0 || std::strcmp(cn, "func_plat2") == 0) && name &&
               brush) {
      // A plat with a name waits up top for what fires it, and only then runs: shut to a ride
      // till then (e1m3's took him down into a pit by a ride that never came back up; Ground
      // Zero's plat that goes either way the same, rmine1's till its soldier falls).
      const int e = edict_of(s, model);
      if (e >= 0) {
        w.locked.push_back(e);
        w.locked_at.push_back(s.sv.edicts[static_cast<size_t>(e)].origin);
        w.locked_turns.push_back(0);
        w.plats.push_back(e);
      }
      continue;
    } else if (brush && name && gate_of(b) >= 0) {
      // A wall with a name standing at the start (a force field something turns off) and one that
      // something blows up: shut to him until they are gone (bunk1's field across the door of its
      // button room, walked into for minutes; the rubble a button blows open). The nav leaves them
      // out of its world (a plain wall it keeps: map_nav's add_walls). A train that waits for its
      // name is shut the same way until it moves (bunk1's over the pit of a secret: a drop through
      // it stood him still for minutes). What fires one opens it (`names_door`).
      const int e = gate_of(b);
      {  // (its rest)
        // A train rests with its least corner on its first corner's place: it may not stand
        // there yet when he starts (it finds its corner a moment after the map spawns).
        sh::Vec3f rest = s.sv.edicts[static_cast<size_t>(e)].origin;
        if (std::strcmp(cn, "func_train") == 0 && target)
          for (const sb::EntityBlock& c : blocks) {
            const char* cname = sb::block_value(c, "targetname");
            const char* corg = sb::block_value(c, "origin");
            if (cname && corg && std::strcmp(cname, target) == 0) {
              const sh::Vec3f lo =
                  !spec.models.empty()
                      ? spec.models[static_cast<size_t>(m)].mins
                      : sh::Vec3f{spec.brushes->models[static_cast<size_t>(m)].mins[0],
                                  spec.brushes->models[static_cast<size_t>(m)].mins[1],
                                  spec.brushes->models[static_cast<size_t>(m)].mins[2]};
              rest = origin_of(corg) - lo;
              break;
            }
          }
        w.locked.push_back(e);
        w.locked_at.push_back(rest);
        w.locked_turns.push_back(0);
        gates.push_back(e);
      }
      continue;
    } else if (std::strcmp(cn, "func_togglewall") == 0 && name && brush) {
      // A wall its name switches off (Scourge of Armagon's force fields: hip1m1's by the button
      // that says "Force field deactivated"): shut to him while it is solid; the nav never saw it,
      // so the links through its box are its crossings.
      // One that starts off and is a floor is a lit bridge (`way_lit_bridge`).
      const int e = edict_of(s, model);
      if (e >= 0 && !way_lit_bridge(w, nav, e)) {
        w.locked.push_back(e);
        w.locked_at.push_back(s.sv.edicts[static_cast<size_t>(e)].origin);
        w.locked_turns.push_back(kLockedGone);
        gates.push_back(e);
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
    } else if (std::strcmp(cn, "func_door") == 0 && !name && brush &&
               way_shot_door(f, edict_of(s, model))) {
      // A door a shot opens (`way_complete_init`'s, its shot a step) stays shut to a walk till it
      // moves, and is shut again once it falls back: hangar1's by the duct, taken as open from the
      // start, stood him in front of it for eight minutes.
      const int e = edict_of(s, model);
      w.locked.push_back(e);
      w.locked_at.push_back(s.sv.edicts[static_cast<size_t>(e)].origin);
      w.locked_turns.push_back(0);
      gates.push_back(e);
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
      // A bridge it slides out into (map_nav.hpp `slides_out`): its nodes are shut till it moves.
      for (const NavBridge& br : nav.bridges)
        if (br.ent == e && !br.floor && !br.turns)
          for (const int32_t n : br.nodes) w.node_door[static_cast<size_t>(n)] = e;
      continue;
    } else if (std::strcmp(cn, "func_door_rotating") == 0 && name && brush) {
      // A turning bridge still turned up is shut to him (its nodes are where it turns down to).
      const int e = edict_of(s, model);
      bool bridge = false;
      for (const NavBridge& br : nav.bridges)
        if (br.turns && br.ent == e) {
          bridge = true;
          w.locked.push_back(e);
          w.locked_at.push_back(br.spawn_angles);
          w.locked_turns.push_back(1);
          for (const int32_t n : br.nodes) w.node_door[static_cast<size_t>(n)] = e;
        }
      // Any other is shut across the links through it till it turns (bunk1's hatch over the
      // ladder up to the top floor, turned open by a button up there: the climb from below met
      // it for minutes).
      // One that starts open (spawnflag 1, base3's) is turned open already: shut by a move.
      const char* rsf = sb::block_value(b, "spawnflags");
      if (!bridge && e >= 0 && !(rsf && (std::atoi(rsf) & 1) != 0)) {
        w.locked.push_back(e);
        w.locked_at.push_back(s.sv.edicts[static_cast<size_t>(e)].angles);
        w.locked_turns.push_back(1);
        gates.push_back(e);
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
    st.node = way_node_in(g, st.lo, st.hi);
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
  // (`complete`) A current a breakable switches off (`way_shot_currents`): the nodes in its push
  // are shut while the breakable stands (hangar1's duct pinned him against its fan for ten
  // minutes).
  for (const ButtonSpot& bs : f.button_spots) {
    if (bs.calm_lo.x == bs.calm_hi.x) continue;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      if (o.x + 16 > bs.calm_lo.x && o.x - 16 < bs.calm_hi.x && o.y + 16 > bs.calm_lo.y &&
          o.y - 16 < bs.calm_hi.y && o.z + 32 > bs.calm_lo.z && o.z - 24 < bs.calm_hi.z)
        w.node_door[n] = bs.ent;
    }
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
    const char* target = nullptr;
    for (const sb::EntityBlock& b : blocks) {
      const char* model = sb::block_value(b, "model");
      if (model && ((be.modelindex > 0 && static_cast<size_t>(be.modelindex) < s.models.size() &&
                     s.models[static_cast<size_t>(be.modelindex)] == model) ||
                    (!bs.model.empty() && bs.model == model))) {
        st.lo = st.hi = spawned_at(b);  // a level kept from before may have it pressed already
        target = sb::block_value(b, "target");
        st.opens = opens(target, st.doors);
        if (target) st.target = target;
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
    // One whose target, or what that fires in turn, is a killbox or a blast no one lives through
    // (biggun's: two relays 22 s on, then the whole base): pressed last, and then out (`dooms`).
    st.dooms = button_dooms(blocks, target);
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
  // (`complete`) A breakable lid over a way (`way_shot_currents`): shut to him till it is gone.
  for (const ButtonSpot& bs : f.button_spots)
    if (bs.lid) gates.push_back(bs.ent);
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
        const sh::Vec3f to = g.nodes[static_cast<size_t>(l.to)].origin;
        // A fall (a drop's, a jump's) runs out at its height and then down: the straight line
        // under its arc missed hip2m2's floor over the pit to the exit, shut till its button far
        // off drops it (a jump from the start's floor down past its lip to the exit below).
        const bool falls =
            (l.kind == sb::NavKind::drop || l.kind == sb::NavKind::jump) && to.z < n.origin.z;
        if (hits(n.origin, to) || (falls && (hits(n.origin, {to.x, to.y, n.origin.z}) ||
                                             hits({to.x, to.y, n.origin.z}, to))))
          w.crossings.push_back({li, static_cast<int32_t>(a), e});
      }
    }
    w.walls.push_back(e);
  }
  way_blast_steps(w, f, nav, s);
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
    if (start >= 0) way_fall_steps(w, nav, s, reach);
  }
  way_exit_links(w, g);  // an exit is walked into only as the step he is on
  way_key_monsters(w, s, blocks);
}

void way_complete_init(Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
                       const ge::shooter::MapSpec& spec) {
  std::vector<sb::EntityBlock> blocks;
  std::string error;
  if (!sb::parse_entity_text(spec.entities.c_str(), blocks, error)) return;
  name_linked_doors(s, spec, blocks);
  const sb::NavGraph& g = nav.graph;
  way_face_shot_buttons(f, nav, s);
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
  way_shot_currents(f, nav, s, spec, blocks);
}

void way_plan(WayThrough& w, Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
              int me, double t) {
  const sb::NavGraph& g = nav.graph;
  const sh::Vec3f self = s.sv.edicts[static_cast<size_t>(me)].origin;
  // At a button of the plan: pressed (or shot) by the fighter's own hands, while a gun he goes
  // for has the plan's say too (hangar1's secret wall, beside him, held his way to the gun past it
  // shut for six minutes: the hands only ever pressed a button the plan was going to).
  if (w.step >= 0 && w.steps[static_cast<size_t>(w.step)].kind == WayKind::button &&
      (w.kind == WayKind::button || w.kind == WayKind::arm)) {
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
    // A door that starts open is open where it spawned (hip2m2's arena, shut by its trigger and
    // opened again by a button beyond it: taken as done while shut, he stayed shut in for good).
    for (const WayThrough::Closer& c : w.closers)
      if (c.door == ent) return dist(s.sv.edicts[static_cast<size_t>(ent)].origin, c.spawn) < 1;
    return true;  // no door of the plan's
  };
  for (WayStep& st : w.steps) {
    if (st.kind == WayKind::button) {
      // A button that opens doors is done while they stand open (one that shuts again, a lift
      // called, is pressed again); one that opens none (a counter's) once it is pressed.
      ButtonSpot& bs = f.button_spots[static_cast<size_t>(st.button)];
      const sh::Edict& be = s.sv.edicts[static_cast<size_t>(bs.ent)];
      // A shot trigger (`model` kept) never moves: answered, it is removed or touches nothing.
      const bool pressed = dist(be.origin, st.lo) > 0.5f || button_looks_pressed(bs, s) ||
                           (!bs.model.empty() && (be.free || be.solid == sh::kSolidNot));
      bool all = !bs.doors.empty();
      for (const int d : bs.doors) all = all && door_open(d);
      // A breakable that calms a current or lids a way (`way_shot_currents`) is done once it is
      // gone.
      const bool calms = bs.calm_lo.x != bs.calm_hi.x || bs.lid;
      const sh::WorldEntity& bw = s.world.entities[static_cast<size_t>(bs.ent)];
      const bool now =
          calms ? bw.free || bw.modelindex <= 0 : pressed || all || (bs.doors.empty() && st.done);
      if (now && !st.done) w.stats.steps_done++;
      st.done = now;
      // One that blows the place up, pressed, is let go of at once: the poke's hold on it kept him
      // at biggun's for 3 s of the 22 he has to get out. So is one shot open: the hold kept him
      // still before hangar1's secret wall through the 3 s it stood open, and it shut on him.
      if ((st.dooms || bs.lid || way_shot_door(f, bs.ent)) && st.done && f.poke == st.button)
        f.poke_until = t;
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
    if (st.done && !was_done && !st.crawl.empty() && !st.fall) {  // back the way he crawled in
      w.crawl_out.assign(st.crawl.rbegin(), st.crawl.rend());
      if (st.node >= 0) w.crawl_out.push_back(g.nodes[static_cast<size_t>(st.node)].origin);
      w.crawl_out_at = t;
    }
  }
  way_wake_gates(w, s);
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
  const bool closed_now = way_closed_floors(w, nav, s);
  // The doors shut now that only something else opens.
  w.barred.clear();
  for (size_t k = 0; k < w.locked.size(); k++) {
    if (locked_moved(w, s, k)) continue;
    // A key door with its key held opens to his touch; without it never, hopeful or not (e4m5's
    // gold doors stood him at them for minutes).
    bool held = false, keyed = false;
    for (const WayThrough::KeyDoor& kd : w.key_doors)
      if (kd.door == w.locked[k]) {
        keyed = true;
        held = held || (items & kd.bit) != 0;
      }
    held = held || way_touch_opened(w, s.sv.edicts[static_cast<size_t>(w.locked[k])], w.locked[k]);
    if (!held && (t >= w.hopeful_until || keyed)) w.barred.push_back(w.locked[k]);
  }
  // A current a breakable still standing calms, and a lid over a way (`way_shot_currents`):
  // barred till it is shot.
  for (const WayStep& st : w.steps)
    if (st.kind == WayKind::button && !st.done) {
      const ButtonSpot& bs = f.button_spots[static_cast<size_t>(st.button)];
      if (bs.calm_lo.x != bs.calm_hi.x || bs.lid) w.barred.push_back(bs.ent);
    }
  // His own routes never through them either (a secret wall by its crossings, below: its sweep's
  // movers stood him still in front of it).
  f.bot.pilot.pilot.shut.clear();
  for (const int32_t b : w.barred)
    if (!is_wall(w, b) && !is_lift(w, f, b)) f.bot.pilot.pilot.shut.push_back(b);
  f.bot.pilot.pilot.hollow = w.cars;  // and a car at rest is walked through
  if (w.barred != w.barred_was || w.hatch_shut != w.hatch_shut_was || closed_now ||
      (!w.exit_links.empty() && w.step != w.exit_step_was)) {  // nor onto a bridge still in
    if (w.barred != w.barred_was) w.shut_trap = way_shut_trap(g, nav.core, w.barred);
    w.barred_was = w.barred;
    w.hatch_shut_was = w.hatch_shut;
    w.exit_step_was = w.step;
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
    for (size_t k = 0; k < w.closed_shut.size(); k++) {  // nor through one closed now
      const sb::NavLink& l = g.links[static_cast<size_t>(w.closed_shut[k])];
      sl.push_back({w.closed_from[k], l.to, l.kind, l.time_s});
    }
    for (const MapNav::Hatch& h : nav.hatches)  // nor through a floor door still resting
      for (const int32_t li : w.hatch_shut)
        if (li == h.link) {
          const sb::NavLink& l = g.links[static_cast<size_t>(li)];
          sl.push_back({h.from, l.to, l.kind, l.time_s});
        }
    way_shut_exits(w, g, sl);  // nor into an exit but the one he is on
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
  way_trigger_sides(w, g);
  if (f.features[sb::Feature::shotplace]) way_shot_sides(w, f, g, s, t);
  // A place is gone to while he can come back from it, or go on from it to an exit: a one-way drop
  // into a pit to touch a trigger or look about held him there for the rest of the map (e1m6's
  // pit, from 13 s; e1m1's flooded secret). Only with nothing else to do is a one-way place gone
  // to (the drop that is the way on).
  reach_back(g, from, w.barred, w.node_door, f.bot.pilot.pilot.set.jumper, w);
  w.back_here.swap(w.returns);
  std::fill(w.back_exit.begin(), w.back_exit.end(), uint8_t{0});
  w.back_exit.resize(g.nodes.size(), 0);
  for (const WayStep& st : w.steps)
    if (st.kind == WayKind::exit && st.node >= 0) {
      reach_back(g, st.node, w.barred, w.node_door, f.bot.pilot.pilot.set.jumper, w);
      for (size_t n = 0; n < g.nodes.size(); n++) w.back_exit[n] |= w.returns[n];
    }
  const auto round_trip = [&](int32_t n) {
    return n >= 0 && (w.back_here[static_cast<size_t>(n)] || w.back_exit[static_cast<size_t>(n)]);
  };
  // A button pressed that blows the place up (`dooms`, pressed last): nothing is left but the exit
  // (below), and no work holds a one-way way to it back: biggun's exit lies past a drop, and the
  // strips the countdown arms, left as work, kept it out of reach until the base blew.
  bool doomed = false;
  for (const WayStep& st : w.steps) doomed = doomed || (st.dooms && st.done);
  bool strict_work = false;
  for (size_t n = 0; n < g.nodes.size() && !strict_work && !w.complete && !doomed; n++)
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
    strict_work = strict_work || (!doomed && !st.done && !st.ever && st.kind != WayKind::exit &&
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
  if (!w.complete) prey = way_key_prey(w, s, g, prey_node, prey_c);  // a door's own monsters
  // A button pressed that blows the place up (`dooms`, pressed last): nothing is left but the exit,
  // the monsters too (biggun's base: he went after one and then a trigger, and was in it at 22 s).
  // Nor a step that opens no door: the triggers its countdown arms (biggun's two strips, armed by
  // the button, drop crates and set off blasts) took him away from the way out until it blew.
  if (doomed) {
    left_to_do = false;
    prey = -1;
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
    if (n < 0) return false;
    const auto i = static_cast<size_t>(n);
    return (i < nav.trap.size() && nav.trap[i]) || (i < w.shut_trap.size() && w.shut_trap[i]);
  };
  const bool me_trapped = trapped_in(from);
  // A key's lock is walked to with a key in hand: none taken yet, it opens nothing.
  bool key_held = false;
  for (const WayStep& st : w.steps) key_held = key_held || (st.kind == WayKind::key && st.done);
  // (`complete`) A step taken last still open (past a drop with no way back) comes before the way
  // back to a map he has played: xsewer2's way on is a drop into a flooded pit from the room he
  // starts in, and he went back the way he came at 25 s with 51 of 53 monsters left.
  bool last_open = false;
  for (const WayStep& st : w.steps)
    last_open = last_open ||
                (w.complete && !way_exit(st) && !st.done && st.node >= 0 && t >= st.left_until &&
                 reach(st.node) && ((!me_trapped && trapped_in(st.node)) || st.drains || st.dooms));
  for (int pass = 0; pass < (w.complete ? 4 : 3) && pick < 0; pass++) {
    // Nothing left but the steps taken last: a way back to a map he has played comes first, before
    // any place not yet seen (he knows the map: the steps are all he needs to find).
    for (size_t k = 0; pass == 2 && !last_open && k < w.steps.size(); k++) {
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
          (st.keyed && !key_held) || (left_to_do && carries(st)) || (w.complete && held_step(st)) ||
          (doomed && !st.opens))
        continue;
      bool covered = false;
      for (const int c : st.covers)
        covered = covered || !locked_moved(w, s, static_cast<size_t>(c));
      if (covered) continue;
      const bool last = (!me_trapped && trapped_in(st.node)) || st.drains || st.dooms;
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
  // A lift he waits for being called by its button (above) is not toured past nor left for a
  // monster: bunk1's sinking floor, waited for in its pit while the tour sent him after one
  // across the map, called again only seven seconds later.
  const bool calling = pick >= 0 && pick == w.calling;
  // A step taken first (a blast weapon, while monsters only a blast puts down wait): the nearest.
  int first = -1;
  for (size_t k = 0; w.complete && pick < 0 && k < w.steps.size(); k++) {
    const WayStep& st = w.steps[k];
    if (!st.first || st.done || st.node < 0 || !reach(st.node) || t < st.left_until) continue;
    if (first < 0 ||
        w.search.cost(st.node) < w.search.cost(w.steps[static_cast<size_t>(first)].node))
      first = static_cast<int>(k);
  }
  if (first >= 0) pick = first;
  if (w.complete && w.toured && left_to_do && !me_trapped && w.out < 0 && !calling && first < 0) {
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
          (st.keyed && !key_held) || st.drains || st.dooms ||
          (!me_trapped && trapped_in(st.node)) || carries(st) || held_step(st))
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
      // The monster he is on stays first: a tour worked out again mid-fight sent him to the next
      // nearest with the last one's health half gone.
      if (w.prey >= 0)
        for (size_t k = 0; k < w.tour.order.size(); k++)
          if (w.tour_stops[static_cast<size_t>(w.tour.order[k] - 1)] == -1 - w.prey) {
            std::rotate(w.tour.order.begin(), w.tour.order.begin() + static_cast<std::ptrdiff_t>(k),
                        w.tour.order.begin() + static_cast<std::ptrdiff_t>(k) + 1);
            break;
          }
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
      calling  ? false
      : toured ? toured == 2
               : (w.complete || pick < 0) && prey >= 0 &&
                     (pick < 0 || way_exit(w.steps[static_cast<size_t>(pick)]) || prey_c < best);
  if (chase) pick = -1;
  // Its time in all, over every spell he chased it: one he keeps turning to between others (e2m6's
  // monster past the pool's bridges, the plat out of the water between) restarts the clock below
  // each time and was chased for the whole map.
  // ONE HE IS HURTING is no waste of time: a hell knight's 250 or a fiend's 300 take a shotgun
  // half a minute, and both clocks stopped it at 16 left and went to the next (e4m5: 8 of 75 in
  // the whole map). Each plan its health fell since the last, the clocks start again.
  bool hurting = false;
  if (chase && prey >= 0 && prey < s.sv.num_edicts) {
    const float hp = s.sv.edicts[static_cast<size_t>(prey)].health;
    hurting = prey == w.prey && hp < w.prey_hp;
    w.prey_hp = hp;
  }
  bool spent = false;
  if (chase && prey >= 0 && static_cast<size_t>(prey) < w.prey_spent.size()) {
    double& sp = w.prey_spent[static_cast<size_t>(prey)];
    if (hurting) sp = 0;
    if (w.prey_tick >= 0 && t - w.prey_tick < 1) sp += t - w.prey_tick;
    spent = sp > kWayPreySpent_s;
    if (spent) sp = 0;
  }
  w.prey_tick = chase ? t : -1;
  if (!spent && (!chase || prey != w.prey || hurting)) {
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
        if (st.tries >= kWayTries) {
          w.stats.given_up++;
          st.given_from = from;
        }
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
      st.given_from = from;
      w.stats.given_up++;
    }
    if (w.log) say_way(w, f, g, s, me, self, st.node, "left");
    w.stats.left++;
    w.step = -1;
    w.plan_at = -1;
  }
  // (`complete`) No shot left with monsters to kill: the nearest gun or ammunition he can fire,
  // unless a step to take first (a blast weapon on a zombie map) is already his.
  const bool takes_first = w.step >= 0 && w.steps[static_cast<size_t>(w.step)].first;
  const int32_t arm = takes_first ? -1 : way_arm(w, f, s, stats_now, !preys.empty(), t, reach);
  const int32_t was = f.objective;
  // Something to do is no idle time (`way_idle`).
  if (arm >= 0 || w.step >= 0 || w.prey >= 0) w.idle_since = way_idle(w.idle_since, true, t);
  if (arm >= 0) {
    w.kind = WayKind::arm;
    f.objective = arm;
  } else if (w.step >= 0) {
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
    // A plan that finds nothing to reach keeps the place he chose and its clock: a door shutting
    // and opening (jail1's) took the place in and out of reach each plan, and choosing it again
    // after every gap started the clock again, so it was never given up.
    if (far != w.explore && far >= 0) {
      w.explore = far;
      w.explore_since = t;
    }
    f.objective = far;
    // Nothing left to do from where he stands for kWayIdle_s (out of a pool onto a ledge whose
    // doors only open from the other side): back into the water, where the swim field leads him
    // out to the map's main part.
    w.idle_since = way_idle(w.idle_since, far >= 0, t);
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
        } else if (way_forgive(st, g, from)) {
          any = true;
          w.stats.forgiven_steps++;
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
    std::string said = line;
    if (w.kind == WayKind::monster) said += way_prey_note(w, f, s, me);
    w.log->push_back(said);
  }
  w.stats.by_kind[static_cast<int>(w.kind)]++;
}

std::vector<uint8_t> way_shut_trap(const sb::NavGraph& g, const std::vector<uint8_t>& core,
                                   const std::vector<int32_t>& barred) {
  const size_t nn = g.nodes.size();
  std::vector<uint8_t> out(nn, 0);
  if (barred.empty() || core.size() != nn) return out;
  const auto open = [&](const sb::NavLink& l) {
    if (sb::nav_blast(l.kind)) return false;
    for (const int32_t b : barred)
      if (l.mover == b) return false;
    return true;
  };
  // Each node's links in, then a walk backward from the core over the open ones.
  std::vector<int32_t> start(nn + 1, 0);
  for (const sb::NavLink& l : g.links)
    if (open(l)) start[static_cast<size_t>(l.to) + 1]++;
  for (size_t n = 0; n < nn; n++) start[n + 1] += start[n];
  std::vector<int32_t> from(static_cast<size_t>(start.back())),
      fill(start.begin(), start.end() - 1);
  for (size_t a = 0; a < nn; a++) {
    const sb::NavNode& n = g.nodes[a];
    for (int32_t k = 0; k < n.links; k++) {
      const sb::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      if (open(l))
        from[static_cast<size_t>(fill[static_cast<size_t>(l.to)]++)] = static_cast<int32_t>(a);
    }
  }
  std::vector<uint8_t> back = core;
  std::vector<int32_t> queue;
  for (size_t n = 0; n < nn; n++)
    if (back[n]) queue.push_back(static_cast<int32_t>(n));
  for (size_t q = 0; q < queue.size(); q++) {
    const auto n = static_cast<size_t>(queue[q]);
    for (int32_t x = start[n]; x < start[n + 1]; x++) {
      const int32_t a = from[static_cast<size_t>(x)];
      if (back[static_cast<size_t>(a)]) continue;
      back[static_cast<size_t>(a)] = 1;
      queue.push_back(a);
    }
  }
  for (size_t n = 0; n < nn; n++) out[n] = back[n] ? 0 : 1;
  return out;
}

}  // namespace ge::shooterbots
