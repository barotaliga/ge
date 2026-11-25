// The way through's bosses and puzzles (way_bosses.hpp).
//
// ge:summary The way through's shot triggers and the lightning boss: its electrodes, bolt, bridge
// ge:keywords way_shot_triggers way_bolt_init way_bolt_pick shot-trigger event-lightning chthon
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooterbots/way_bosses.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "shooter/trace.hpp"
#include "shooterbots/buttons.hpp"

namespace ge::shooterbots {

namespace {

namespace sh = ge::shooter;
namespace sb = ge::shooterbots;

constexpr float kWayShotTriggerFrom = 1200;  // a shot trigger is shot from this near, units

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

sh::Vec3f origin_of(const char* org) {
  sh::Vec3f o;
  char* end = nullptr;
  o.x = std::strtof(org, &end);
  o.y = std::strtof(end, &end);
  o.z = std::strtof(end, &end);
  return o;
}

// The edict the rules made of inline model `model`, or -1.
int edict_of(const ge::shooterbots::BotView& s, const char* model) {
  for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0 || static_cast<size_t>(d.modelindex) >= s.models.size())
      continue;
    if (s.models[static_cast<size_t>(d.modelindex)] == model) return e;
  }
  return -1;
}

// The doors (entities) named by what `target` fires, through relays.
std::vector<int> doors_fired(const ge::shooterbots::BotView& s,
                             const std::vector<sb::EntityBlock>& blocks, const char* target) {
  std::vector<int> out;
  const std::vector<std::string> fired = button_fires(blocks, target);
  for (const sb::EntityBlock& d : blocks) {
    const char* dc = sb::block_value(d, "classname");
    const char* dn = sb::block_value(d, "targetname");
    const char* dm = sb::block_value(d, "model");
    if (!dc || !dn || !dm || std::strcmp(dc, "func_door") != 0) continue;
    for (const std::string& n : fired)
      if (n == dn)
        if (const int door = edict_of(s, dm); door >= 0) out.push_back(door);
  }
  return out;
}

}  // namespace

void way_shot_triggers(Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
                       const ge::shooter::MapSpec& spec,
                       const std::vector<sb::EntityBlock>& blocks) {
  const sb::NavGraph& g = nav.graph;
  // Quake's trigger with health answers only a shot (e4m3's window, far across the start room, is
  // the only way out of it; he waited by the shut doors for the whole map). The rules keep no
  // model on a trigger's edict: it is found by its box.
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* model = sb::block_value(b, "model");
    const char* health = sb::block_value(b, "health");
    const char* target = sb::block_value(b, "target");
    if (!cn || !model || model[0] != '*' || !health || std::atof(health) <= 0 || !target ||
        (std::strcmp(cn, "trigger_once") != 0 && std::strcmp(cn, "trigger_multiple") != 0))
      continue;
    const int m = std::atoi(model + 1);
    sh::Vec3f lo, hi;
    if (spec.brushes) {
      if (m <= 0 || static_cast<size_t>(m) >= spec.brushes->models.size()) continue;
      const ge::brush::Model& bm = spec.brushes->models[static_cast<size_t>(m)];
      lo = {bm.mins[0], bm.mins[1], bm.mins[2]};
      hi = {bm.maxs[0], bm.maxs[1], bm.maxs[2]};
    } else {
      if (m <= 0 || static_cast<size_t>(m) >= spec.models.size()) continue;
      lo = spec.models[static_cast<size_t>(m)].mins;
      hi = spec.models[static_cast<size_t>(m)].maxs;
    }
    int e = -1;
    for (int k = s.sv.set.maxclients + 1; k < s.sv.num_edicts && e < 0; k++) {
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(k)];
      if (!d.free && d.solid != sh::kSolidNot && dist(d.absmin, lo) < 4 && dist(d.absmax, hi) < 4)
        e = k;
    }
    if (e < 0) continue;
    const std::vector<std::string> fired = button_fires(blocks, target);
    ButtonSpot bs;
    bs.ent = e;
    bs.at = (lo + hi) * 0.5f;
    bs.shoot = true;
    bs.model = model;
    for (const sb::EntityBlock& d : blocks) {
      const char* dc = sb::block_value(d, "classname");
      const char* dn = sb::block_value(d, "targetname");
      const char* dm = sb::block_value(d, "model");
      if (!dc || !dn || !dm || std::strcmp(dc, "func_door") != 0) continue;
      for (const std::string& n : fired)
        if (n == dn)
          if (const int door = edict_of(s, dm); door >= 0) bs.doors.push_back(door);
    }
    // One that opens no door (e2m7's seven, counted by a trigger_counter that opens the way on)
    // is still shot: done once it answered (gone, or touching nothing).
    // Shot from the nearest node whose eye line reaches it (a window is shot through the room).
    float best_d = kWayShotTriggerFrom;
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
    if (bs.node >= 0) f.button_spots.push_back(std::move(bs));
  }
}

// (`complete`) Quake's Chthon, killed only by the bolt the map's `event_lightning` throws between
// its two electrodes (the doors whose target is `lightning`) while both have moved: each
// electrode's buttons, the button that fires the bolt, and the doors the boss's death opens.
void way_bolt_init(WayThrough& w, const Fighter& f, const ge::shooterbots::BotView& s,
                   const std::vector<sb::EntityBlock>& blocks) {
  WayThrough::Bolt& b = w.bolt;
  b = {};
  const char* bolt_name = nullptr;
  for (const sb::EntityBlock& d : blocks) {
    const char* dc = sb::block_value(d, "classname");
    const char* dt = sb::block_value(d, "target");
    const char* dm = sb::block_value(d, "model");
    if (dc && std::strcmp(dc, "event_lightning") == 0) bolt_name = sb::block_value(d, "targetname");
    if (dc && dt && dm && std::strcmp(dc, "func_door") == 0 && std::strcmp(dt, "lightning") == 0)
      if (const int e = edict_of(s, dm); e >= 0) {
        b.electrodes.push_back(e);
        b.electrodes_at.push_back(s.sv.edicts[static_cast<size_t>(e)].origin);
      }
    if (dc && dt && std::strcmp(dc, "monster_boss") == 0)
      for (const int e : doors_fired(s, blocks, dt)) {
        b.after.push_back(e);
        b.after_at.push_back(s.sv.edicts[static_cast<size_t>(e)].origin);
      }
  }
  // What wakes him: the steps standing on a thing whose target is his name (e1m7's rune).
  for (const sb::EntityBlock& d : blocks) {
    const char* dc = sb::block_value(d, "classname");
    const char* boss = sb::block_value(d, "targetname");
    if (!dc || !boss || std::strcmp(dc, "monster_boss") != 0) continue;
    for (const sb::EntityBlock& o : blocks) {
      const char* ot = sb::block_value(o, "target");
      const char* oo = sb::block_value(o, "origin");
      if (!ot || !oo || std::strcmp(ot, boss) != 0) continue;
      const sh::Vec3f at = origin_of(oo);
      for (size_t k = 0; k < w.steps.size(); k++) {
        const WayStep& st = w.steps[k];
        if (st.kind != WayKind::button && at.x >= st.lo.x - 32 && at.x <= st.hi.x + 32 &&
            at.y >= st.lo.y - 32 && at.y <= st.hi.y + 32 && at.z >= st.lo.z - 32 &&
            at.z <= st.hi.z + 32)
          b.wake.push_back(static_cast<int>(k));
      }
    }
  }
  if (!bolt_name || b.electrodes.size() != 2 || b.after.empty()) {
    b = {};
    return;
  }
  b.plates.resize(b.electrodes.size());
  for (size_t k = 0; k < w.steps.size(); k++) {
    const WayStep& st = w.steps[k];
    if (st.kind != WayKind::button || st.button < 0) continue;
    const int ent = f.button_spots[static_cast<size_t>(st.button)].ent;
    const sh::Edict& be = s.sv.edicts[static_cast<size_t>(ent)];
    for (const sb::EntityBlock& d : blocks) {
      const char* dm = sb::block_value(d, "model");
      const char* dt = sb::block_value(d, "target");
      if (!dm || !dt || be.modelindex <= 0 ||
          static_cast<size_t>(be.modelindex) >= s.models.size() ||
          s.models[static_cast<size_t>(be.modelindex)] != dm)
        continue;
      for (const std::string& n : button_fires(blocks, dt))
        if (n == bolt_name) b.fire = static_cast<int>(k);
      for (const int e : doors_fired(s, blocks, dt))
        for (size_t i = 0; i < b.electrodes.size(); i++)
          if (b.electrodes[i] == e) b.plates[i].push_back(static_cast<int>(k));
    }
  }
  if (b.fire < 0 || b.plates[0].empty() || b.plates[1].empty()) b = {};
}

// (`complete`) The step the lightning boss asks for now, or -1: nothing while his death's doors
// have moved; an electrode at rest, its nearest button; both moved, the bolt's button.
int way_bolt_pick(const WayThrough& w, const ge::shooterbots::BotView& s) {
  const WayThrough::Bolt& b = w.bolt;
  if (b.fire < 0) return -1;
  for (const int k : b.wake)
    if (!w.steps[static_cast<size_t>(k)].done) return -1;  // not woken yet
  for (size_t i = 0; i < b.after.size(); i++)
    if (dist(s.sv.edicts[static_cast<size_t>(b.after[i])].origin, b.after_at[i]) >= 1) return -1;
  for (size_t i = 0; i < b.electrodes.size(); i++) {
    if (dist(s.sv.edicts[static_cast<size_t>(b.electrodes[i])].origin, b.electrodes_at[i]) >= 1)
      continue;
    int best = -1;
    double best_c = 0;
    for (const int k : b.plates[i]) {
      const int32_t n = w.steps[static_cast<size_t>(k)].node;
      if (n < 0 || !w.search.reached(n)) continue;
      const double c = w.search.cost(n);
      if (best < 0 || c < best_c) {
        best = k;
        best_c = c;
      }
    }
    return best;
  }
  return b.fire;
}

bool way_shot_only(const EntityBlock& b) {
  const char* hp = block_value(b, "health");
  return hp != nullptr && std::atof(hp) > 0;
}

}  // namespace ge::shooterbots
