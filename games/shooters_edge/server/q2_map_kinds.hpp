// Quake II's movers' and triggers' kinds, states and actions, and a team's frame (q2_map.cpp's
// own vocabulary, in a header so the file stays under the line cap).
//
// ge:summary Quake II's movers' kinds, states and think actions, and a team moved as one
// ge:keywords q2 movers triggers mover-kinds mover-state think-actions team-movers
// ge:skill shooter
// ge:test tests/shooters_edge_q2_map_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server::q2map {

// The movers' and triggers' kinds, from their class names.
enum Class : uint8_t {
  kNone,
  kDoor,
  kButton,
  kPlat,
  kTrain,
  kWall,
  kTimer,
  kTriggerOnce,
  kTriggerMultiple,
  kRelay,
  kAlways,
  kPush,
  kHurt,
  kTeleporter,
  kChangeLevel,
  kSpeaker,
  kExplosion,
  kSecret,
  kGoal,
  kDoorField,
  kPlatField,
  kDoorRotating,
  kRotating,
  kAreaPortal,
  kLight,
  kHelp,
  kCrossTrigger,
  kCrossTarget,
  kElevator,
  kSecretDoor,
};

// A mover's states: at rest at its first place, going to its second, at its second, coming back.
enum State : uint8_t { kAtFirst, kGoing, kAtSecond, kComing };

// The actions a think or an arrival runs.
enum Act : uint8_t {
  kNoAct,
  kDoorClose,
  kDoorTop,
  kDoorBottom,
  kButtonIn,
  kButtonReturn,
  kButtonOut,
  kPlatDown,
  kPlatTop,
  kPlatBottom,
  kTrainFind,
  kTrainNext,
  kTrainCorner,
  kTimerFire,
  kAlwaysFire,
  kRemove,
  kCrossCheck,
  kPlatUp,  // a plat stepped on in coop, rising after coop_elevator_delay (saves keep these)
  // A secret door's steps (secret_use): out to the side, a pause, back along its arrow, the
  // wait, the way home in two moves.
  kSecretAside,
  kSecretOut,
  kSecretOpen,
  kSecretIn,
  kSecretBeside,
  kSecretHome,
  kSecretShut,
};

struct Mover {
  Class cls = kNone;
  int corner = -1;  // a train's current corner
};

// A team of movers (doors or trains sharing `team`) moves as one: a frame one part is blocked
// undoes every part's move and all they carried, delays every part's next think by a frame, and
// none of them steps; a frame none is, they step after the last has moved (q2_map_team.cpp).
struct TeamFrame {
  int master = -1;
  bool blocked = false;
  std::vector<Q2Entity> parts;                         // the team as it was
  std::vector<std::pair<sh::Vec3f, sh::Vec3f>> where;  // every entity's place and angles
  std::vector<std::pair<ge::classic::Q2State, sh::Vec3f>> players;  // moves and view angles
  std::vector<int> ground;
  std::vector<size_t> steps;  // the parts whose step is due once the team has moved
};

// Starts team `master`'s frame: what to put back should a part be blocked.
void team_begin(const Q2Server& s, TeamFrame& t, int master);

// A part of team `t` was blocked: everything back as it was, every part's think a frame later.
void team_undo(Q2Server& s, TeamFrame& t);

}  // namespace shooters_edge::server::q2map
