// SHOOTER'S EDGE'S LOOK: the shooter plugin's picture settings and looks (shooter/app/look.hpp:
// r_look, the r_ and ce_ cvars, the world's and the post's options), read here under the game's
// own name, and what joins them to the game's effects: the moment's lights kept by kind, the
// screen's tint as the player tones it.
//
// ge:summary Shooter's Edge's look: the plugin's picture settings joined to the game's effects
// ge:keywords look r_rocketlight r_explosionlight r_powerupglow kept_lights toned_blend
// ge:keywords v_contentblend gl_cshiftpercent gl_polyblend
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#pragma once

#include <span>
#include <vector>

#include "classic/app/look.hpp"
#include "shooters_edge/app/effects.hpp"

namespace shooters_edge::look {

using namespace ge::classic::look;  // NOLINT(google-build-using-namespace): the game's own name

// The screen's tint (effects::screen_blend) of `s` as the player tones it: each layer at its v_
// share, all of them at gl_cshiftpercent, none with gl_polyblend 0.
void toned_blend(const Picture& picture, const effects::Screen& s, double rgba[4]);
// The moment's lights the player keeps: `lights` less each kind its setting puts out
// (r_rocketlight, r_explosionlight, r_powerupglow), into `out` (cleared first); `lights` itself
// when every kind is on, `out` otherwise.
std::span<const effects::Light> kept_lights(const Picture& picture,
                                            std::span<const effects::Light> lights,
                                            std::vector<effects::Light>& out);

}  // namespace shooters_edge::look
