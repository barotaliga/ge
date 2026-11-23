// SHOOTER'S EDGE'S LOOK: the shooter plugin's picture settings and looks (shooter/app/look.hpp:
// r_look, the r_ and ce_ cvars, the world's and the post's options), read here under the game's
// own name, and what joins them to the game's effects: the moment's lights kept by kind, the
// screen's tint as the player tones it.
//
// ge:summary Shooter's Edge's look: the plugin's picture settings joined to the game's effects
// ge:keywords look r_rocketlight r_explosionlight r_powerupglow kept_lights toned_blend
// ge:keywords v_contentblend gl_cshiftpercent gl_polyblend toned_tints
// ge:keywords gl_damageblend_frac damage-frame edge-blend
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
// THE TWO TINTS the post pass lays (render/post.hpp): `flat` over the whole view and `edge`, a
// hit's flash as a frame round it at gl_damageblend_frac (`Picture::damage_frac`; 0: the flash
// stays in `flat` and `edge` is clear). Quake's (`rules` false): `s` toned (toned_blend), the
// flash split off it. Quake II's (`rules`): the rules' own tint as they sent it, `whole`, or where
// they kept the flash apart (`damage`'s share above 0) and the frame is on, the rest (`apart`) and
// the flash; gl_cshiftpercent and gl_polyblend on both. The colours are sRGB-encoded, 0..1.
struct Tints {
  double flat[4] = {0, 0, 0, 0}, edge[4] = {0, 0, 0, 0};
};
Tints toned_tints(const Picture& picture, const effects::Screen& s, bool rules,
                  const double whole[4], const double apart[4], const double damage[4]);
// The moment's lights the player keeps: `lights` less each kind its setting puts out
// (r_rocketlight, r_explosionlight, r_powerupglow), into `out` (cleared first); `lights` itself
// when every kind is on, `out` otherwise.
std::span<const effects::Light> kept_lights(const Picture& picture,
                                            std::span<const effects::Light> lights,
                                            std::vector<effects::Light>& out);

}  // namespace shooters_edge::look
