// THE CLASSIC VIEW OF A RECORDING OR A GAME JOINED: the first game's bob, strafe roll and damage
// kick drawn on a demo's eye and on a server's, as the window draws them on its own listen server
// (present.hpp). A recording keeps the player's place and look, never the view the player saw, so
// the window rebuilds it: engine/camera's classic rig (`classic_first_person`, the player's
// `cl_bob`, `cl_rollangle`, `v_kickpitch`... over it) on a subject made from the frame.
//
// - THE FEET: the recorded eye less its view height, less the box's 24 units under the origin.
// - THE VELOCITY: a game joined, its prediction's (`DemoBody`); a client recording, the player's
//   state as sent (the protocol's velocity, a part left out 0); else (a multiview recording, a
//   single-player demo) the change of the player's place between the last two frames READ over
//   their time, a move over `kMostStepUnits` (a teleport, a respawn) taken as no move.
// - ON THE GROUND: a game joined, its prediction's; else the player's box traced `kGroundProbe`
//   units down through the level's player hull, on the ground when it lands on a floor no
//   steeper than the move's (`kFloorNormal`); with no level, when the velocity has no climb.
// - THE BLOWS (the damage message to the player, `DemoState::blows`) tip the view toward where
//   they came from by their count (half the armour and the blood, at least `kKickCountMin`), each
//   once (by `DemoState::blows_taken`), however many frames were read since the last look.
// - DEAD the eye drops to 16 units under the origin and the view rolls 80 degrees; gibbed, 8 over.
// - Not drawn by the rig: the sequel's recordings (their server worked the view out, `kick`), the
//   intermission, a frame with no player to ride: `demo_view` as read.
// A seek (the demo's clock going back), a new level or another player tracked starts the rig over.
//
// ge:summary The classic bob, strafe roll and damage kick on a demo's or a joined server's eye
// ge:keywords demo-eyes demo_eyes_view classic-view view-bob strafe-roll damage-kick demo
// ge:keywords joined-server multiview client-recording velocity on-ground ground-trace blows
// ge:skill camera shooter
// ge:test tests/shooters_edge_demo_eyes_test.cpp
#pragma once

#include <cstdint>

#include "camera/first_person.hpp"
#include "render/view.hpp"
#include "shooter/brush_hull.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

inline constexpr float kGroundProbe = 2;      // units: how far under the feet a floor counts
inline constexpr float kFloorNormal = 0.7f;   // the steepest floor stood on (its normal's z)
inline constexpr float kMostStepUnits = 64;   // a frame's move farther than this is no move
inline constexpr double kMostEyesStep = 0.2;  // the rig's longest step, s

// What a game joined knows of its own player better than a recording (its prediction).
struct DemoBody {
  bool known = false;
  sh::Vec3f velocity;  // units/s
  bool on_ground = true;
};

struct DemoEyes {
  ge::camera::FirstPersonState eyes;
  double clock = -1;         // the demo's clock at the last look, s (-1: none)
  int64_t frames_seen = -1;  // the frame read at the last look
  int64_t blows_seen = 0;    // of DemoState::blows_taken
  int levels = -1, player = -2;
  sh::Vec3f feet;         // the player's feet in the last frame read, units
  double feet_time = -1;  // its time, s
  sh::Vec3f velocity;     // estimated from the frames read, units/s
  bool was_grounded = true;
  float fall_speed = 0;  // the last look's vertical speed, units/s
};

// True when the rig draws this frame (a first-game recording, a player to ride, no intermission).
bool demo_eyes_drawn(const DemoState& s);

// The view at the demo's clock `clock_s` of `drawn` (the frame as shown, between frames) read off
// `read` (the last frame read: the blows, the place the velocity is estimated from) with `row` (the
// player's camera cvars over the classic row), the level's world model (`world`: its player hull
// traced for the ground; nullptr none) and what a joined game knows (`body`). `fov_y` the lens.
ge::render::FrameView demo_eyes_view(const DemoState& drawn, const DemoState& read, DemoEyes& e,
                                     const ge::camera::FirstPersonRow& row,
                                     const sh::BrushModel* world, const DemoBody& body,
                                     double fov_y, double clock_s, double time_s);

}  // namespace shooters_edge::client
