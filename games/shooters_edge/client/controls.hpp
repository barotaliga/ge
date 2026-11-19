// THE PLAYER'S HANDS: one frame's keys and mouse, the shooter plugin's (shooter/controls.hpp), as
// the view's turn and the command the listen server runs. The keys are the console's binds (`bind w
// +forward`): `add_cvars` registers the moves as buttons (+forward, +back, +moveleft, +moveright,
// +moveup, +movedown, +jump, +attack, +speed, +strafe, +left, +right, +attack2: the quick weapon's
// second key, quick_fire.hpp; +rjump and +rrjump: the rocket jump helpers, a press asking the
// server for one rocket jump, down or back, with the jump in the same command:
// shooter/rjump_assist.hpp) and `impulse <n>`, and `default_binds` gives every key the classic
// layout has unbound (W A S D and the arrows move, SPACE jumps, the left button fires, ALT
// sidesteps with the mouse, 1..8 pick a weapon, the wheel the next one carried, F12 takes a
// screenshot: engine/capture/screenshot.hpp; no key walks or crouches), before the player's config
// rebinds them. OPTIONS ▸ CUSTOMIZE CONTROLS rebinds them too.
//
// - `button_moves` reads the buttons (the window); `key_moves` reads the default keys straight off
//   the input state (a scripted session, a test), so a session's frames make the same commands on
//   any machine whatever the player bound.
// - `read_controls` makes the command: at the run's speeds with `cl_run` on (the default) and
//   +speed, bound to no key, to walk; the other way round with it off; the left button fires.
// - `turn_view`: raw mouse counts turn the view (`sensitivity` degrees a count, `m_invert` flips
//   the pitch), the pitch held within `pitch_limit`; +left and +right turn at `cl_yawspeed`; with
//   +strafe held (or `lookstrafe` on) the mouse's sideways counts sidestep instead of turning.
// - `lookspring`: while it is on, a player who moves on the ground without pitching the mouse has
//   his pitch drift to the program's ideal pitch (`spring_view`: level ground centres it, a slope
//   tilts it down or up the way he goes, at `kSpringSpeed` degrees a second).
// - `aim_turn_view`: a player the server's aim help helps sees his view turn toward the player a
//   shot was bent onto while he fires, and back to his own aim after (`cl_aimhelp_turn`, on by
//   default; shooter/aim_turn.hpp), so his recording looks like his own aim.
//
// ge:summary The player's keys and mouse as the view's turn and the server's command
// ge:keywords controls keys mouse-look raw-mouse sensitivity invert wasd jump fire impulse
// ge:keywords weapon-keys wheel read_controls turn_view move-cmd m_invert cvars
// ge:keywords lookspring spring_view ideal-pitch centre-view always-run cl_run
// ge:keywords lookstrafe attack2
// ge:keywords buttons binds default_binds forward attack speed strafe impulse key_moves
// ge:keywords movedown moveup swim-down
// ge:keywords in_mouse_release mouse-capture
// ge:skill input shooter
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <span>
#include <string_view>

#include "console/console.hpp"
#include "shooter/controls.hpp"
#include "shooters_edge/client/quick_fire.hpp"
#include "shooters_edge/client/weapon_choice.hpp"
#include "shooters_edge/client/weapon_groups.hpp"

namespace shooters_edge::client {

using ge::shooter::aim_turn_view;
using ge::shooter::button_moves;
using ge::shooter::ControlButtons;
using ge::shooter::key_moves;
using ge::shooter::kSpringSpeed;
using ge::shooter::Moves;
using ge::shooter::read_controls;
using ge::shooter::release_buttons;
using ge::shooter::spring_view;
using ge::shooter::turn_view;

// The shooter's controls with this game's quick weapon and weapon groups beside them.
struct ControlSettings : ge::shooter::Controls {
  QuickFire quick;      // the quick weapon's cvars and state (quick_fire.hpp)
  WeaponGroups groups;  // the number keys' weapon groups (weapon_groups.hpp)
  WeaponChoice choice;  // `weapon`, preselection and hiding (weapon_choice.hpp)
  // The score keys (`+showscores`, `+showteamscores`; TAB runs the first by default).
  ge::console::Button* showscores = nullptr;
  ge::console::Button* showteamscores = nullptr;
};

// The weapon keys made into `m`'s impulse and attack off what the server last said (`s`), at `t`
// s: the weapon command and preselection (weapon_choice.hpp), the game's weapon groups
// (weapon_groups.hpp), then the quick weapon (quick_fire.hpp).
void weapon_moves(ControlSettings& c, std::span<const std::string_view> game_groups,
                  const hud::Status& s, double t, Moves& m);

// Whether the players' board's key (+showscores) or the teams' (+showteamscores) is held.
bool scores_held(const ControlSettings& c);
bool team_scores_held(const ControlSettings& c);

// The shooter's control cvars and buttons over `c` (add_control_cvars), the quick weapon's and
// the weapon groups' cvars, the command `impulse` and the score keys (`c` must outlive `con`).
void add_cvars(ge::console::Console& con, ControlSettings& c);
// Every move's default key bound where the key runs nothing yet (call before the configs run).
void default_binds(ge::console::Console& con);

}  // namespace shooters_edge::client
