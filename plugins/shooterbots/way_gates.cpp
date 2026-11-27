// The way through's waking triggers (way_gates.hpp).
//
// ge:summary Triggers a button wakes: the names that wake one, touch doors, the near side's node
// ge:keywords way_gate_names way_wake_gates way_touch_opened way_trigger_sides triggered gate
// ge:keywords deathtarget gate_dead
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooterbots/way_gates.hpp"

#include <cmath>
#include <cstdlib>
#include <string>

#include "shooter/trace.hpp"
#include "shooterbots/buttons.hpp"
#include "shooterbots/way_parts.hpp"

namespace ge::shooterbots {

namespace sh = ge::shooter;

bool trigger_waits(const EntityBlock& b) {
  const char* sf = block_value(b, "spawnflags");
  return sf != nullptr && (std::atoi(sf) & kTriggerWaits) != 0;
}

void way_gate_names(WayStep& st, const char* name, const std::vector<EntityBlock>& blocks,
                    const BotView& s) {
  constexpr size_t kMostNames = 8;  // a chain of wakers is a few names long
  st.gate.emplace_back(name);
  for (size_t k = 0; k < st.gate.size() && st.gate.size() < kMostNames; k++)
    for (const EntityBlock& by : blocks) {
      const char* bt = block_value(by, "target");
      const char* bn = block_value(by, "targetname");
      if (!bt || !bn || st.gate[k] != bt) continue;
      bool known = false;
      for (const std::string& gn : st.gate) known = known || gn == bn;
      if (!known) st.gate.emplace_back(bn);
    }
  // A monster whose death fires one of the names (biggun's boss: its death wakes the strip by the
  // only door north), found by its spot among the monsters standing at the start.
  constexpr float kSpawnNear = 64;  // a monster this near its block's spot is that block's, units
  for (const EntityBlock& by : blocks) {
    const char* dt = block_value(by, "deathtarget");
    const char* org = block_value(by, "origin");
    if (!dt || !org) continue;
    bool gates = false;
    for (const std::string& gn : st.gate) gates = gates || gn == dt;
    if (!gates) continue;
    const sh::Vec3f at = way_parts::origin_of(org);
    int best = -1;
    float best_d = kSpawnNear;
    for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
      const bool monster = (d.flags & sh::kFlagMonster) != 0 || d.solid == sh::kSolidSlidebox;
      if (d.free || !monster || d.health <= 0) continue;
      const sh::Vec3f dd = d.origin - at;
      const float far = sh::length(dd);
      if (far < best_d) {
        best = e;
        best_d = far;
      }
    }
    if (best >= 0) st.gate_dead.push_back(best);
  }
  st.left_until = kWayForever;
}

void way_wake_gates(WayThrough& w, const BotView& s) {
  for (WayStep& st : w.steps) {
    if (st.gate.empty()) continue;
    bool live = false;
    for (const WayStep& o : w.steps)
      for (const std::string& gn : st.gate) live = live || ((o.done || o.ever) && o.target == gn);
    for (const int e : st.gate_dead) {
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
      live = live || d.free || d.health <= 0;
    }
    if (live) {
      st.gate.clear();
      st.left_until = -1;
    }
  }
}

bool way_touch_opened(const WayThrough& w, const sh::Edict& de, int32_t e) {
  constexpr float kAcross = 32;  // the trigger's box this near the door's counts as on it
  for (const WayStep& st : w.steps) {
    // (Not one a key's lock answers: base3's blue door, walked to before its key and waited at
    // for minutes.)
    if (st.kind != WayKind::trigger || !st.repeats || st.keyed || !st.gate.empty() ||
        st.left_until >= kWayForever)
      continue;
    bool its = false;
    for (const int d : st.doors) its = its || d == e;
    if (its && st.lo.x < de.absmax.x + kAcross && st.hi.x > de.absmin.x - kAcross &&
        st.lo.y < de.absmax.y + kAcross && st.hi.y > de.absmin.y - kAcross &&
        st.lo.z < de.absmax.z + kAcross && st.hi.z > de.absmin.z - kAcross)
      return true;
  }
  return false;
}

void way_trigger_sides(WayThrough& w, const NavGraph& g) {
  for (WayStep& st : w.steps) {
    if (st.kind != WayKind::trigger || st.done || st.doors.empty() || st.node < 0 ||
        w.search.reached(st.node))
      continue;
    const sh::Vec3f mid = (st.lo + st.hi) * 0.5f;
    int32_t best = -1;
    float best_d = 0;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      // a player's box there meets the trigger's
      if (!w.search.reached(static_cast<int32_t>(n)) || o.x + 16 < st.lo.x || o.x - 16 > st.hi.x ||
          o.y + 16 < st.lo.y || o.y - 16 > st.hi.y || o.z + 32 < st.lo.z || o.z - 24 > st.hi.z)
        continue;
      const sh::Vec3f d = o - mid;
      const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
      if (best < 0 || dd < best_d) {
        best = static_cast<int32_t>(n);
        best_d = dd;
      }
    }
    if (best >= 0) st.node = best;
  }
}

void way_shot_sides(WayThrough& w, Fighter& f, const NavGraph& g, const BotView& s, double t) {
  constexpr double kSoughtEvery_s = 5;  // the search traces many lines: not at every plan
  for (WayStep& st : w.steps) {
    if (st.kind != WayKind::button || st.done || st.button < 0 || st.node < 0 ||
        w.search.reached(st.node) || t - st.shot_sought < kSoughtEvery_s)
      continue;
    st.shot_sought = t;
    ButtonSpot& bs = f.button_spots[static_cast<size_t>(st.button)];
    if (!bs.shoot) continue;
    int32_t best = -1;
    float best_d = kShotFrom * kShotFrom;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      if (!w.search.reached(static_cast<int32_t>(n))) continue;
      const sh::Vec3f o = g.nodes[n].origin;
      const sh::Vec3f d = o - bs.at;
      const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
      if (dd >= best_d) continue;
      // The eye and the muzzle either side of it see it, with room to spare: a line that grazes
      // the door it is shot through misses (the shot leaves the gun, not the eye).
      constexpr float kOffsets[5][3] = {
          {0, 0, 22}, {8, 0, 14}, {-8, 0, 14}, {0, 8, 14}, {0, -8, 14}};
      bool clear = true;
      for (const auto& off : kOffsets) {
        const sh::Vec3f from{o.x + off[0], o.y + off[1], o.z + off[2]};
        const sh::HullTrace tr =
            sh::trace_move(s.sv, from, {}, {}, bs.at, sh::kTraceNoMonsters, -1);
        clear = clear && (tr.fraction >= 1 || tr.ent == bs.ent);
        if (!clear) break;
      }
      if (!clear) continue;
      best = static_cast<int32_t>(n);
      best_d = dd;
    }
    if (best >= 0) st.node = bs.node = best;
  }
}

}  // namespace ge::shooterbots
