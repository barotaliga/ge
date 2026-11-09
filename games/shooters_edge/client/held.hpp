// THE WEAPON IN THE PLAYER'S HAND, frame by frame: the game's held weapon (view/hand.hpp's hand,
// the switch, the shot's kick and flash) driven by what the listen server says. Presentation only:
// it reads the player's entity and writes nothing back.
//
// - THE SET (`load_held_set`): a classic game's own weapons from its files when one is loaded,
//   Shooter's Edge's own otherwise; never the one standing in for the other.
// - THE WEAPON is the one the HUD shows (view.hpp's `status_of`; the HUD's order is the hand's).
// - A SHOT is the mod's `attack_finished` moved past the server's time: a weapon that fires sets
//   it to its refire ahead, and a spawn sets it to now, so a spawn is never taken for a shot.
// - THE PLACE: the eye's look, the speed along the ground and whether the player stands on it (the
//   stride's clock runs while he walks), from the view and the player's state.
//
// ge:summary The weapon in hand: whose set, which one, a shot, the look, the stride
// ge:keywords held-weapon hand step_held attack_finished shot weapon-switch viewmodel stride
// ge:keywords load_held_set classic-weapons game-weapons
// ge:skill shooter render
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <span>

#include <string>
#include <vector>

#include "render/view.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/view/hand.hpp"

namespace shooters_edge::client {

struct HeldState {
  view::Hand hand;
  float attack_finished = 0;  // the last frame's
  double stride_s = 0;        // the stride's clock, s
  int shots = 0;              // shots seen
  bool shown = false;         // the last frame drew a weapon (a dead recorder's sinks from it)
};

// The held weapons the window draws into `out`. With a classic game's folder open (`files`), only
// that game's own (`game.held_weapons`): when its files lack one, no weapon is drawn and the line
// says why, never Shooter's Edge's own in its place (a classic game is played with its own
// things alone). Without one, Shooter's Edge's own set from the first of `dirs` that holds it.
// True when the set is the game's (authored at the eye: HeldState's `hand.at_eye`).
bool load_held_set(const GameAdapter& game, const SearchPath& files,
                   std::span<const std::string> dirs, std::vector<ge::viewmodel::HeldModel>& out);

// The hand after the wall frame of `dt` s at time `t` s, frame_view having brought `v` up to date.
// `models` is load_weapon_models's set (none: the hand still switches and kicks, with no flash).
view::HandFrame step_held(const Listen& l, const ViewState& v, HeldState& s,
                          const view::ViewSettings& settings,
                          std::span<const ge::viewmodel::HeldModel> models, double t, double dt);

// The hand in a recorded game whose frame is `frame` (timedemo.hpp's demo_view): the weapon the
// recording's HUD shows at the view's eye and look; not drawn while the recorder holds no view
// model (a spectator) or the level has ended; when the recorder dies, the weapon sinks out of view.
// shortcut: no shot and no stride (the recording's weapon frame and velocity are not read), so the
// weapon neither kicks, flashes nor bobs; read them for that.
view::HandFrame step_demo_held(const DemoState& s, HeldState& h, const view::ViewSettings& settings,
                               std::span<const ge::viewmodel::HeldModel> models,
                               const ge::render::FrameView& frame, double t, double dt);

}  // namespace shooters_edge::client
