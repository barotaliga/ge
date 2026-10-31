// THE PLAYER'S HANDS: one frame's keyboard and mouse (engine/input's state) as the view's turn and
// the command the listen server runs. Raw mouse counts turn the view (`sensitivity` degrees a
// count, `invert` flips the pitch), the pitch held within `pitch_limit`; W A S D move at the
// run's speeds, SPACE jumps, the left button fires, 1..8 pick a weapon (the mod's impulses) and
// the wheel takes the next one carried (impulse 10). A pure function of the state: a scripted
// session's frames make the same commands on any machine. `add_cvars` makes the mouse's two
// settings the cvars `sensitivity` and `m_invert`, which the console, a config and the main menu
// set alike, and `lookspring`: while it is on, a player who moves on the ground without pitching
// the mouse has his pitch drift to the program's ideal pitch (`spring_view`: level ground
// centres it, a slope tilts it down or up the way he goes, at `kSpringSpeed` degrees a second).
//
// ge:summary The player's keys and mouse as the view's turn and the server's command
// ge:keywords controls keys mouse-look raw-mouse sensitivity invert wasd jump fire impulse
// ge:keywords weapon-keys wheel read_controls turn_view move-cmd m_invert cvars
// ge:keywords lookspring spring_view ideal-pitch centre-view
// ge:skill input shooter
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include "console/console.hpp"
#include "input/state.hpp"
#include "shooter/pmove.hpp"

namespace shooters_edge::client {

struct ControlSettings {
  double sensitivity = 0.066;  // degrees of turn a raw count
  bool invert = false;
  bool lookspring = false;
  float pitch_limit = 89;   // degrees up or down
  int forward_speed = 400;  // units/s
  int side_speed = 350;
  int up_speed = 200;
};

inline constexpr double kSpringSpeed = 90;  // degrees a second the lookspring drifts the pitch

// `sensitivity`, `m_invert` and `lookspring` over `c` (which must outlive `con`).
void add_cvars(ge::console::Console& con, ControlSettings& c);

// The view (degrees: pitch down positive, yaw) turned by this frame's mouse.
void turn_view(const ge::input::InputState& in, const ControlSettings& c, ge::shooter::Vec3f& view);

// The pitch drifted toward `ideal` (degrees) for `dt` s when `lookspring` is on, the player on
// the ground and moving (`cmd`), and the mouse has not pitched the view this frame.
void spring_view(const ge::input::InputState& in, const ControlSettings& c, bool on_ground,
                 const ge::shooter::MoveCmd& cmd, float ideal, double dt, ge::shooter::Vec3f& view);

// The command this frame's keys and buttons make (no length, no angles: the server's frame and the
// view give those). An impulse is in `cmd.impulse` (0 none).
ge::shooter::MoveCmd read_controls(const ge::input::InputState& in, const ControlSettings& c);

}  // namespace shooters_edge::client
