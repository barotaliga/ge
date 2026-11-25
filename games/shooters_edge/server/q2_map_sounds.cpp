// The movers' sounds and loops (q2_map.hpp states the rule; q2_map_kinds.hpp names the calls).
//
// ge:summary Quake II's movers' sounds: the sound list, a door's samples, the loops while they move
// ge:keywords q2 movers sound-list door-sounds loop-sounds speakers teleporter-hum
// ge:skill shooter
// ge:test tests/shooters_edge_q2_map_test.cpp
#include <algorithm>
#include <string>

#include "shooters_edge/server/q2_map_kinds.hpp"

namespace shooters_edge::server::q2map {

// A door's start or end sample; null when it moves quietly. A door of water sounds of water,
// and only when it has `sounds`. Only a team's master sounds.
const char* door_sample(const Q2Server& s, const Q2Entity& e, bool start) {
  if (e.team_master >= 0 && e.team_master != static_cast<int>(&e - s.entities.data()))
    return nullptr;
  if (e.classname == "func_water") {
    if (e.sounds == 0) return nullptr;
    return start ? "world/mov_watr.wav" : "world/stp_watr.wav";
  }
  if (e.sounds == 1) return nullptr;
  return start ? kDoorStart : kDoorEnd;
}

// The sound list's index of `name`, put on the list when it is not there (as the map starts,
// before the list is told to anyone).
int sound_index(Q2Server& s, const std::string& name) {
  if (name.empty()) return 0;
  const auto found = std::find(s.sounds.begin(), s.sounds.end(), name);
  if (found != s.sounds.end()) return static_cast<int>(found - s.sounds.begin());
  s.sounds.push_back(name);
  return static_cast<int>(s.sounds.size()) - 1;
}

// The movers', the pads' and the speakers' loops, and the one-shots the movers make beside them,
// on the sound list (porting note: q2-render-fx.md § Entity loop sounds). A door grinds on
// unless its `sounds` is 1 (a door of water only splashes); a plat always; a train its `noise`;
// the pad hums; a looping speaker plays its `noise`, on from the start with spawn flag 1.
void map_loops(Q2Server& s) {
  for (Q2Entity& e : s.entities) {
    e.loop = 0;
    if (e.gone) continue;
    switch (class_of_name(e.classname)) {
      case kDoor:
      case kDoorRotating:
        for (const char* one : {door_sample(s, e, true), door_sample(s, e, false)})
          if (one != nullptr) sound_index(s, one);
        if (e.classname != "func_water" && e.sounds != 1) e.loop = sound_index(s, kDoorMove);
        break;
      case kPlat:
      case kPlat2:
        sound_index(s, kPlatStart);
        sound_index(s, kPlatEnd);
        e.loop = sound_index(s, kPlatMove);
        break;
      case kTrain:
        e.loop = sound_index(s, e.noise);
        break;
      case kButton:
        if (e.sounds != 1) sound_index(s, kButtonSound);
        break;
      case kTeleporter:
        if (e.pad == 1) e.loop = sound_index(s, kPadHum);
        if (e.pad == 1) sound_index(s, kTeleSound);
        break;
      case kSpeaker:
        if ((e.spawnflags & 3) == 0) {
          sound_index(s, e.noise);
          break;
        }
        e.loop = sound_index(s, e.noise);
        e.on = (e.spawnflags & 1) != 0;
        break;
      default:
        break;
    }
  }
}

// The loop entity `e` (at `i`) makes this frame: a mover only while it moves, and only its team's
// first; a speaker while on; a pad always.
int loop_now(const Q2Entity& e, int i) {
  if (e.loop == 0 || e.gone) return 0;
  switch (class_of_name(e.classname)) {
    case kDoor:
    case kDoorRotating:
    case kPlat:
    case kPlat2:
    case kTrain:
      if (e.team_master >= 0 && e.team_master != i) return 0;
      return e.arrive != kNoAct ? e.loop : 0;
    case kSpeaker:
      return e.on ? e.loop : 0;
    default:
      return e.loop;
  }
}

}  // namespace shooters_edge::server::q2map
