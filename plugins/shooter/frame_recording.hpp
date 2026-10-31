// THE RECORDED STEPS the original move (framemove.hpp) is held to: scripted runs over the classic
// move's hand-built worlds (the scenes of recording.hpp's format, read from its recording), one
// line a step with the state measured after it. `parse_frame_recording` reads the text format;
// `replay_frames` runs a scenario's steps through `frame_think`, `frame_program_move` and
// `frame_move` on a server holding only the scene's world and the player, carrying our own state,
// and measures each line within the move's tolerance (`kPositionTolerance`, `kVelocityTolerance`)
// with the discrete fields equal and `teleport_time` within the position tolerance.
//
// HOW A SCENARIO RUNS: the server's time starts at 1.0 and advances by `dt` after each step; the
// player stands up from the scenario's start (the player's box, health 100, the eye 22 up, the
// jump released, not on the ground, `oldorigin` the origin) with the water level taken once
// before the first step; nothing hurts it.
//
// The format, a line each (whitespace-separated, `#` comments, blank lines skipped):
//   scene <name>                         (a scene of the classic move's recording)
//   scenario <name>                      (on the last scene)
//   settings key=value ...               (gravity friction edgefriction stopspeed maxspeed
//                                         accelerate maxvelocity nostep rollangle rollspeed)
//   start origin x y z velocity x y z dt t
//   frame i view pitch yaw move f s u jump 0|1 -> origin x y z velocity x y z onground 0|1
//     waterlevel w watertype t jumpreleased 0|1 waterjump 0|1 teleport t clip c
// The committed copy is tests/golden/shooter_framemove.txt.
//
// ge:summary The recorded original-move steps: their text format and a replay measured line by line
// ge:keywords framemove recording vectors parity scenario replay_frames parse_frame_recording
// ge:keywords tolerance original-move sv_playermove
// ge:skill shooter testing
// ge:test tests/shooter_framemove_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "shooter/framemove.hpp"
#include "shooter/recording.hpp"

namespace ge::shooter {

struct FrameState {
  Vec3f origin, velocity;
  bool onground = false;
  int waterlevel = 0;
  int watertype = kContentsEmpty;
  bool jumpreleased = false;
  bool waterjump = false;
  float teleport = 0;
  int clip = 0;
};

struct FrameLine {
  int index = 0;
  FrameCommand cmd;
  bool jump = false;
  FrameState after;
};

struct FrameScenario {
  std::string name;
  std::string scene;  // a scene of the classic move's recording, by name
  FrameMoveSettings settings;
  float gravity = 800, maxvelocity = 2000;
  Vec3f origin, velocity;
  double dt = 0;
  std::vector<FrameLine> lines;
};

struct FrameRecording {
  std::vector<FrameScenario> scenarios;
};

// Reads the text format. On a malformed line returns false with `error` naming the line.
bool parse_frame_recording(std::string_view text, FrameRecording& out, std::string& error);

struct FrameCheck {
  int index = 0;
  FrameState ours;
  float position_error = 0;  // the largest component's difference, units
  float velocity_error = 0;  // units/s
  std::string mismatch;      // the discrete fields that differ
  bool ok = true;
};

struct FrameReplay {
  std::string name;
  std::vector<FrameCheck> lines;
  float max_position_error = 0, max_velocity_error = 0;
  int worst_position_line = -1, worst_velocity_line = -1;
  int failed = 0;
  std::string error;  // the scene was not found
};

// Entity `e` made the recordings' player at `origin` (frame_recording.hpp § How a scenario runs).
void stand_frame_player(Server& sv, int e, Vec3f origin, Vec3f velocity);

// The scenario over its scene, found by name in `scenes` (the classic move's recording).
FrameReplay replay_frames(const Recording& scenes, const FrameScenario& scenario);

}  // namespace ge::shooter
