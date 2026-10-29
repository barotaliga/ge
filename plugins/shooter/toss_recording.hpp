// THE RECORDED FLIGHTS the toss rules are held to: point-sized thrown entities (a bouncing grenade,
// a missile, a tossed gib, a flying nail) against a hand-built world's hull 0, one line a physics
// frame, measured from the classic loop. `parse_toss_recording` reads the text format;
// `replay_toss` runs a flight through `physics_toss` (physics.hpp) frame by frame on a server
// holding only that world and the entity, carrying our own state, and measures each line against
// the recording within the move's tolerance (`kPositionTolerance`, `kVelocityTolerance`), with on
// the ground and hit equal and the trace's fraction and plane normal within 1e-4.
//
// The format, a line each (whitespace-separated, `#` comments, blank lines skipped):
//   scene <name> / brush <solid|water|slime|lava> / plane nx ny nz dist / end
//   toss <name> movetype <bounce|flymissile|toss|fly> origin x y z velocity x y z
//     frametime t gravity g maxvelocity m        (on the last scene)
//   frame i -> origin x y z velocity x y z onground 0|1 hit 0|1 fraction f normal x y z
//   end
// The committed copy is tests/golden/shooter_toss_recording.txt.
//
// ge:summary The recorded thrown entities: their text format and a replay measured frame by frame
// ge:keywords toss recording vectors entity-vectors parity grenade rocket gib nail bounce
// ge:keywords parse_toss_recording replay_toss tolerance
// ge:skill shooter testing
// ge:test tests/shooter_toss_recording_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "shooter/recording.hpp"

namespace ge::shooter {

constexpr float kTraceTolerance = 1e-4f;  // a trace's fraction and normal

struct TossFrame {
  Vec3f origin, velocity;
  bool onground = false;
  bool hit = false;
  float fraction = 1;
  Vec3f normal;
};

struct TossFlight {
  std::string name;
  int scene = 0;  // into TossRecording::scenes
  int movetype = 0;
  Vec3f origin, velocity;
  double frametime = 0;
  float gravity = 800;
  float maxvelocity = 2000;
  std::vector<TossFrame> frames;  // the recorded state after each frame
};

struct TossRecording {
  std::vector<RecordedScene> scenes;
  std::vector<TossFlight> flights;
};

// Reads the text format. On a malformed line returns false with `error` naming the line.
bool parse_toss_recording(std::string_view text, TossRecording& out, std::string& error);

struct TossCheck {
  int index = 0;
  TossFrame ours;
  float position_error = 0;  // the largest component's difference, units
  float velocity_error = 0;  // units/s
  float trace_error = 0;     // the fraction's and the normal's largest difference
  std::string mismatch;      // the discrete fields that differ
  bool ok = true;
};

struct TossReplay {
  std::string name;
  std::vector<TossCheck> frames;
  float max_position_error = 0, max_velocity_error = 0, max_trace_error = 0;
  int worst_position_frame = -1, worst_velocity_frame = -1;
  int failed = 0;
};

TossReplay replay_toss(const TossRecording& rec, const TossFlight& flight);

}  // namespace ge::shooter
