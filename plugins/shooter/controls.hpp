// THE PLAYER'S HANDS: one frame's keys and mouse as the view's turn and the command the listen
// server runs. The keys are the console's binds (`bind w +forward`): `add_control_cvars` registers
// the moves as buttons (+forward, +back, +moveleft, +moveright, +moveup, +movedown, +jump, +attack,
// +speed, +strafe, +left, +right, +attack2: the quick weapon's second key, quick_fire.hpp; +rjump
// and +rrjump: the rocket jump helpers, a press asking the server for one rocket jump, down or
// back, with the jump in the same command: shooter/rjump_assist.hpp).
// A game adds its own words (a mod's `impulse <n>`), its default keys and the quick weapon's and
// weapon groups' settings beside them.
//
// - `button_moves` reads the buttons (the window); `key_moves` reads the default keys straight off
//   the input state (a scripted session, a test), so a session's frames make the same commands on
//   any machine whatever the player bound.
// - `read_controls` makes the command: each key asks for its speed (`cl_forwardspeed`,
//   `cl_backspeed`, `cl_sidespeed`, `cl_upspeed`), times `cl_movespeedkey` running; `cl_run` on
//   (the default) runs and +speed, bound to no key, walks; the other way round with it off; the
//   server caps what it is asked. With +klook held the forward and back keys look instead. With
//   `freelook` off and +mlook not held the mouse's up and down moves you (`m_forward`) instead of
//   looking. The left button fires.
// - `turn_view`: raw mouse counts, shaped by `m_filter` and `m_accel` (mouse_feel.hpp), turn the
//   view (`sensitivity` times `m_yaw` degrees a count sideways, times `m_pitch` up and down, as a
//   classic config writes them; `m_invert` flips the pitch), the pitch held within `pitch_limit`;
//   +left and +right turn at `cl_yawspeed`; with +strafe held (or `lookstrafe` on) the mouse's
//   sideways counts sidestep instead of turning; +lookup and +lookdown (and +klook's move keys)
//   pitch at `cl_pitchspeed`; +speed held turns the keys `cl_anglespeedkey` times faster;
//   `centerview` levels the pitch.
// - `lookspring`: while it is on, a player who moves on the ground without pitching the mouse has
//   his pitch drift to the program's ideal pitch (`spring_view`: level ground centres it, a slope
//   tilts it down or up the way he goes, at `kSpringSpeed` degrees a second).
// - `aim_turn_view`: a player the server's aim help helps sees his view turn toward the player a
//   shot was bent onto while he fires, and back to his own aim after (`cl_aimhelp_turn`, on by
//   default; shooter/aim_turn.hpp), so his recording looks like his own aim.
//
// ge:summary Any shooter's keys and mouse: the move buttons, mouse look, cl_run, lookspring
// ge:keywords controls keys mouse-look raw-mouse sensitivity invert wasd jump fire read_controls
// ge:keywords turn_view move-cmd m_invert lookspring spring_view ideal-pitch always-run cl_run
// ge:keywords lookstrafe attack2 buttons forward attack speed strafe key_moves movedown moveup
// ge:keywords in_mouse_release mouse-capture cl_yawspeed add_control_cvars cl_forwardspeed
// ge:keywords cl_backspeed cl_sidespeed cl_upspeed cl_movespeedkey cl_anglespeedkey cl_pitchspeed
// ge:keywords freelook mlook klook lookup lookdown centerview force_centerview m_forward zoom zoom-key
// ge:skill input shooter
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <string>

#include "console/console.hpp"
#include "input/mouse_capture.hpp"
#include "input/state.hpp"
#include "shooter/aim_turn.hpp"
#include "shooter/mouse_feel.hpp"
#include "shooter/pmove.hpp"

namespace ge::shooter {

// The console's buttons the moves are read from (null until `add_cvars` registers them).
struct ControlButtons {
  console::Button *forward = nullptr, *back = nullptr, *moveleft = nullptr, *moveright = nullptr,
                  *moveup = nullptr, *movedown = nullptr, *jump = nullptr, *attack = nullptr,
                  *speed = nullptr, *strafe = nullptr, *left = nullptr, *right = nullptr,
                  *attack2 = nullptr;
  // Looking by key: +lookup, +lookdown (cl_pitchspeed), +klook (the move keys look), +mlook (the
  // mouse looks up and down with freelook off).
  console::Button *lookup = nullptr, *lookdown = nullptr, *klook = nullptr, *mlook = nullptr;
  bool center = false;  // `centerview` asked: the pitch levelled on the next turn
  // The rocket jump helpers (server/rjump_rules.hpp): a press asks once, with a jump.
  console::Button *rjump = nullptr, *rrjump = nullptr;
  int impulse = 0;  // the last `impulse <n>` not yet sent (0 none)
};

struct Controls {
  double sensitivity = 3;  // the mouse's speed, a classic config's number (`yaw_per_count`)
  double m_yaw = 0.022;    // degrees a count at sensitivity 1, sideways
  double m_pitch = 0.022;  // the same up and down
  bool invert = false;
  bool lookspring = false;
  bool run = true;               // cl_run: run without +speed (+speed walks)
  bool lookstrafe = false;       // the mouse's sideways counts sidestep, not turn
  double yaw_speed = 140;        // cl_yawspeed: +left and +right, degrees a second
  double pitch_speed = 150;      // cl_pitchspeed: +lookup, +lookdown and +klook, degrees a second
  double angle_speed_key = 1.5;  // cl_anglespeedkey: the turn keys' rate times this with +speed
  bool freelook = true;          // the mouse always looks up and down (else only with +mlook)
  double m_forward = 1;          // the mouse's up and down moves you, freelook off: units a count
  float pitch_limit = 89;        // degrees up or down
  // The speeds the keys ask for, units/s, walking (cl_forwardspeed, cl_backspeed, cl_sidespeed,
  // cl_upspeed); running asks for them times cl_movespeedkey. The server caps them.
  int forward_speed = 400;
  int back_speed = 400;
  int side_speed = 400;
  int up_speed = 400;
  double move_speed_key = 2;
  double strafe_scale = 36;  // units/s of sidestep a degree the mouse's counts would have turned
  bool aim_turn = true;      // cl_aimhelp_turn: the view turns onto the aim help's bend
  // in_mouse_release: what takes the pointer back from the look (input::Release's bits)
  uint32_t mouse_release = input::kReleaseDefault;
  ControlButtons buttons;
  // m_filter, m_accel and its kin, in_raw (mouse_feel.hpp); the filter's memory changes as the
  // view turns, so it is the one part a turn writes.
  mutable MouseFeel mouse;
  // +zoom held: the lens narrowed to kZoomFov and the sensitivity scaled by kZoomFov over the lens
  // it had; -zoom puts back both values as they were (their text kept here).
  struct Zoomed {
    bool on = false;
    std::string fov, sensitivity;
  } zoom;
};

// +zoom's lens, degrees across at 4:3, at once (no easing).
inline constexpr double kZoomFov = 35;

// One frame's moves: what is held, and the impulse asked for.
struct Moves {
  bool forward = false, back = false, moveleft = false, moveright = false;
  bool moveup = false, movedown = false;  // swim up and down; down crouches where the move ducks
  bool jump = false, attack = false, speed = false, strafe = false;
  bool attack2 = false;  // the second fire key (quick_fire.hpp); a plain attack where unread
  bool left = false, right = false;  // turning
  bool lookup = false, lookdown = false, klook = false, mlook = false;
  bool center = false;  // `centerview`: level the pitch
  int impulse = 0;
};

// The aim help's turn (shooter/aim_turn.hpp) for one frame, when `cl_aimhelp_turn` is on: the
// view moved toward the player the server last bent a shot onto (`target_at` his middle, null when
// gone), and back once the attack is let go; off, any turn left is given back at once.
void aim_turn_view(const Controls& c, AimTurn& turn, Vec3f& view, Vec3f eye, const Vec3f* target_at,
                   bool attack, double dt);

inline constexpr double kSpringSpeed = 90;  // degrees a second the lookspring drifts the pitch

// The cvars `sensitivity`, `m_yaw`, `m_pitch`, `m_invert`, `lookspring`, `cl_aimhelp_turn`,
// `cl_run`, `lookstrafe`, `in_mouse_release`, `cl_yawspeed`, `cl_pitchspeed`, `cl_anglespeedkey`,
// `freelook`, `m_forward` and the keys' speeds (`cl_forwardspeed`, `cl_backspeed`,
// `cl_sidespeed`, `cl_upspeed`, `cl_movespeedkey`) and the mouse's feel (mouse_feel.hpp) over `c`,
// the moves' buttons into `c.buttons` and the commands `centerview` and `force_centerview` (`c`
// must outlive `con`).
void add_control_cvars(console::Console& con, Controls& c);

// Degrees the view turns a raw count sideways (`sensitivity` times `m_yaw`). A `sensitivity` under
// 0.5 is read as degrees a count itself, the window's own saves before it took a classic
// config's number.
double yaw_per_count(const Controls& c);
// The same up and down (`m_pitch`).
double pitch_per_count(const Controls& c);

// The moves the buttons hold; the impulse waiting is taken.
Moves button_moves(Controls& c);
// Every button let go (the menu or the console took the keys).
void release_buttons(Controls& c);
// The moves the default keys make, read off the input state: W A S D, SPACE, the left button,
// 1..8 and the wheel.
Moves key_moves(const input::InputState& in);

// The view (degrees: pitch down positive, yaw) turned by this frame's mouse and turn keys over
// `dt` s.
void turn_view(const input::InputState& in, const Moves& m, const Controls& c, double dt,
               Vec3f& view);
// The same with the default keys (key_moves) and no turn keys.
void turn_view(const input::InputState& in, const Controls& c, Vec3f& view);

// The pitch drifted toward `ideal` (degrees) for `dt` s when `lookspring` is on, the player on
// the ground and moving (`cmd`), and the mouse has not pitched the view this frame.
void spring_view(const input::InputState& in, const Controls& c, bool on_ground, const MoveCmd& cmd,
                 float ideal, double dt, Vec3f& view);

// The command `m` makes (no length, no angles: the server's frame and the view give those); the
// mouse's sideways counts sidestep when +strafe or `lookstrafe` says so. An impulse is in
// `cmd.impulse` (0 none).
MoveCmd read_controls(const Moves& m, const input::InputState& in, const Controls& c);
// The same with the default keys (key_moves).
MoveCmd read_controls(const input::InputState& in, const Controls& c);

}  // namespace ge::shooter
