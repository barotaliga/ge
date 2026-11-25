// Ground Zero's changes to the base game's map things (q2_gz.hpp): the toggled trigger_multiple,
// the silent earthquake, the rotating door and the explosive woken by their first use (the
// pushes are q2_gz_triggers.cpp's). q2_map.cpp and
// q2_targets.cpp ask here only while the pack is played.
//
// ge:summary Ground Zero's changes to base map things: toggles, silent ones, inactive doors
// ge:keywords q2 ground-zero rogue trigger-multiple toggle silent
// ge:keywords target-earthquake map-things spawnflags inactive func-door-rotating func-explosive
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_test.cpp
#include <string>

#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

bool gz_toggled(const Q2Server& s, const Q2Entity& e) {
  return s.opt.pack == Q2Pack::ground_zero && (e.spawnflags & 8) != 0 &&
         e.classname == "trigger_multiple";
}

bool gz_inactive(const Q2Server& s, const Q2Entity& e) {
  return s.opt.pack == Q2Pack::ground_zero && e.classname == "func_door_rotating" &&
         (e.spawnflags & 8192) != 0;
}

bool gz_used(Q2Server& s, Q2Entity& e) {
  // A func_door_rotating with INACTIVE (8192): the first use wakes it, a door like any other from
  // then on (its touch field live, or shot open when it has health), which no trigger uses again.
  if (gz_inactive(s, e)) {
    e.spawnflags = (e.spawnflags & ~8192) | kGzWoken;
    const int master = e.team_master >= 0 ? e.team_master : -1;
    for (Q2Entity& f : s.entities)
      if (f.classname == "door_field" && f.owner >= 0 && f.owner == master) f.trigger = true;
    return true;
  }
  if (e.classname == "func_door_rotating" && (e.spawnflags & kGzWoken) != 0) return true;
  // A named func_explosive with INACTIVE (8): the first use makes it one shot can break; a later
  // use blows it.
  if (e.classname == "func_explosive" && (e.spawnflags & 8) != 0 && !e.targetname.empty() &&
      (e.spawnflags & kGzWoken) == 0) {
    e.spawnflags |= kGzWoken;
    return true;
  }
  // A trigger_multiple with TOGGLE (8): each use switches it on or off, and never fires it.
  if (!gz_toggled(s, e)) return false;
  e.trigger = !e.trigger;
  e.state = e.trigger ? 1 : 0;
  return true;
}

bool gz_silent(const Q2Server& s, const Q2Entity& e) {
  if (s.opt.pack != Q2Pack::ground_zero) return false;
  return e.classname == "target_earthquake" && (e.spawnflags & 1) != 0;
}

}  // namespace shooters_edge::server
