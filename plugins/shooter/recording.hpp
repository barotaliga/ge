// THE RECORDED MOVES the classic move is held to: hand-built worlds (`scene`, convex brushes),
// and scripted runs over them (`scenario`: the settings, the start, other solids, then one line
// a command with the state measured after it). `parse_recording` reads the text format;
// `replay` runs a scenario's commands through `player_move` from its start, carrying our own
// state from line to line, and measures each line against the recording.
//
// THE TOLERANCE: the recordings are 32-bit floats printed to 9 significant digits; a faithful
// float move agrees within `kPositionTolerance` in position and `kVelocityTolerance` in velocity
// on every line (the largest component's difference), with the discrete fields equal (on the
// ground and the solid stood on, the water level and type, the jump held, the slide's blocked
// bits, the touch list) and the water jump's clock within the position tolerance.
//
// The format, a line each (whitespace-separated, `#` comments):
//   scene <name> / brush <solid|water|slime|lava> / plane nx ny nz dist / end
//   scenario <name> (on the last scene) / movevars key=value ... /
//   start origin x y z velocity x y z pm_type <type> onground 0|1 jump_held 0|1 waterjumptime t /
//   physent box origin x y z mins x y z maxs x y z /
//   cmd i msec m angles p y r move f s u buttons b -> origin x y z velocity x y z onground 0|1
//     groundent g waterlevel w watertype t jump_held 0|1 waterjumptime t blocked b touch e... /
//   end
// The committed copy is tests/golden/shooter_recording.txt.
//
// ge:summary The recorded classic moves: their text format, and a replay measured line by line
// ge:keywords recording vectors parity scenario scene replay tolerance parse_recording replay
// ge:keywords test-vectors golden compare
// ge:skill shooter testing
// ge:test tests/shooter_recording_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "shooter/brush_hull.hpp"
#include "shooter/pmove.hpp"

namespace ge::shooter {

constexpr float kPositionTolerance = 0.01f;  // units
constexpr float kVelocityTolerance = 0.05f;  // units/s

struct RecordedScene {
  std::string name;
  std::vector<Brush> brushes;
};

// The state a recording prints after a command.
struct RecordedState {
  Vec3f origin, velocity;
  bool onground = false;
  int groundent = -1;
  int waterlevel = 0;
  int watertype = kContentsEmpty;
  bool jump_held = false;
  float waterjump_time = 0;
  int blocked = 0;
  std::vector<int> touches;
};

struct RecordedLine {
  int index = 0;
  MoveCmd cmd;
  RecordedState after;
};

struct RecordedBox {
  Vec3f origin, mins, maxs;
};

struct RecordedScenario {
  std::string name;
  int scene = 0;  // into Recording::scenes
  MoveSettings settings;
  MoveState start;
  std::vector<RecordedBox> boxes;  // solids 1, 2, ... after the world
  std::vector<RecordedLine> lines;
};

struct Recording {
  std::vector<RecordedScene> scenes;
  std::vector<RecordedScenario> scenarios;
};

// Reads the text format. On a malformed line returns false with `error` naming the line.
bool parse_recording(std::string_view text, Recording& out, std::string& error);

// A world's hulls built once from a scene's brushes, and the move's list over them.
struct SceneWorld {
  HullData point_hull, clip_hull;
  MoveWorld world;
};
// The list points into the hulls' heap arrays, which a move keeps and a copy does not: never
// copy a SceneWorld (moving one is fine).
void build_scene_world(const RecordedScene& scene, const std::vector<RecordedBox>& boxes,
                       SceneWorld& out);

// One line measured: our state after the command against the recording's.
struct LineCheck {
  int index = 0;
  RecordedState ours;
  float position_error = 0;  // the largest component's difference, units
  float velocity_error = 0;  // units/s
  std::string mismatch;      // the discrete fields that differ ("" when all agree)
  bool ok = true;            // within the tolerance and every discrete field equal
};

struct ReplayResult {
  std::string name;
  std::vector<LineCheck> lines;
  float max_position_error = 0;
  float max_velocity_error = 0;
  int worst_position_line = -1;  // the cmd index of the worst line
  int worst_velocity_line = -1;
  int failed = 0;  // lines not ok
};

ReplayResult replay(const Recording& rec, const RecordedScenario& scenario);

}  // namespace ge::shooter
