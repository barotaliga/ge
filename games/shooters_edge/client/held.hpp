// THE WEAPON IN THE PLAYER'S HAND, frame by frame: the game's held weapon (view/hand.hpp's hand,
// the switch, the shot's kick and flash) driven by what the listen server says. Presentation only:
// it reads the player's entity and writes nothing back.
//
// - THE WEAPON is the one the HUD shows (view.hpp's `status_of`; the HUD's order is the hand's).
// - A SHOT is the mod's `attack_finished` moved past the server's time: a weapon that fires sets
//   it to its refire ahead, and a spawn sets it to now, so a spawn is never taken for a shot.
// - THE PLACE: the eye's look, the speed along the ground and whether the player stands on it (the
//   stride's clock runs while he walks), from the view and the player's state.
//
// ge:summary The weapon in hand from the listen server: which one, a shot, the look, the stride
// ge:keywords held-weapon hand step_held attack_finished shot weapon-switch viewmodel stride
// ge:skill shooter render
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <span>

#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/view/hand.hpp"

namespace shooters_edge::client {

struct HeldState {
  view::Hand hand;
  float attack_finished = 0;  // the last frame's
  double stride_s = 0;        // the stride's clock, s
  int shots = 0;              // shots seen
};

// The hand after the wall frame of `dt` s at time `t` s, frame_view having brought `v` up to date.
// `models` is load_weapon_models's set (none: the hand still switches and kicks, with no flash).
view::HandFrame step_held(const Listen& l, const ViewState& v, HeldState& s,
                          const view::ViewSettings& settings,
                          std::span<const ge::viewmodel::HeldModel> models, double t, double dt);

}  // namespace shooters_edge::client
