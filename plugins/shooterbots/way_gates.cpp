// The way through's waking triggers (way_gates.hpp).
//
// ge:summary Triggers a button wakes: the names that wake one, touch doors, the near side's node
// ge:keywords way_gate_names way_wake_gates way_touch_opened way_trigger_sides triggered gate
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooterbots/way_gates.hpp"

#include <cmath>
#include <cstdlib>
#include <string>

namespace ge::shooterbots {

namespace sh = ge::shooter;

bool trigger_waits(const EntityBlock& b) {
  const char* sf = block_value(b, "spawnflags");
  return sf != nullptr && (std::atoi(sf) & kTriggerWaits) != 0;
}

void way_gate_names(WayStep& st, const char* name, const std::vector<EntityBlock>& blocks) {
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
  st.left_until = kWayForever;
}

void way_wake_gates(WayThrough& w) {
  for (WayStep& st : w.steps) {
    if (st.gate.empty()) continue;
    bool live = false;
    for (const WayStep& o : w.steps)
      for (const std::string& gn : st.gate) live = live || ((o.done || o.ever) && o.target == gn);
    if (live) {
      st.gate.clear();
      st.left_until = -1;
    }
  }
}

bool way_touch_opened(const WayThrough& w, const sh::Edict& de, int32_t e) {
  constexpr float kAcross = 32;  // the trigger's box this near the door's counts as on it
  for (const WayStep& st : w.steps) {
    if (st.kind != WayKind::trigger || !st.repeats || !st.gate.empty() ||
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

}  // namespace ge::shooterbots
