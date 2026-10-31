// WHAT THE WINDOW THROWS INTO THE WORLD EACH FRAME: the game's effects (app/effects.hpp) read off
// what the listen server did, for the world pass (world_draw.hpp) to draw. Presentation only: it
// reads the server's state and the view's record of it (present.hpp's missiles and blasts) and
// writes nothing back.
//
// - A missile that flew since the last frame lays its trail over the stretch it flew (a rocket's
//   or a grenade's embers and smoke) and a rocket glows as it goes.
// - An explosion the mod announced bursts once and lights the walls while its light fades.
// - A campaign's temporary entities and particle bursts (Listen::effects): a shot's or a spike's
//   hit sparks back toward the eye, blood falls, a teleport flares, lava splashes, a beam lights
//   and sparks where it ends.
// - THE SCREEN'S BLEND: the liquid round the eye (the player's `waterlevel` and `watertype`), and
//   a hit's flash, fading: in a campaign off the blows the program reported (Listen::blows: the
//   blood red, what the armour took a duller amber), in a match off the health lost.
//
// Every burst draws on its own seed from a counter, so the same run throws the same sparks.
//
// ge:summary The window's effects each frame: trails, explosions, their lights, the screen's blend
// ge:keywords world-fx effects trail rocket-trail explosion dynamic-light screen-blend hurt-flash
// ge:keywords underwater medium step_fx fx-state temp-entity gunshot spike teleport-splash beam
// ge:skill render shooter
#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "camera/subject.hpp"
#include "render/explosions.hpp"
#include "shooters_edge/app/effects.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/world_models.hpp"

namespace shooters_edge::client {

struct FxState {
  std::unordered_map<int, ge::camera::Vec3> last;  // each missile's place a frame ago, m
  std::unordered_map<int, ge::camera::Vec3> next;  // this frame's, swapped into `last`
  std::size_t explosions_seen = 0;                 // of Listen::explosions
  std::vector<Blast> lit;                          // the explosions whose light still fades
  double hurt = 0;                                 // the hit's flash now (effects::Screen::hurt)
  double armour = 0;                               // the armour's share of it (Screen::armour)
  std::size_t blows_seen = 0;                      // of Listen::blows
  std::size_t effects_seen = 0;                    // of Listen::effects
  float health = 0;                                // the last frame's
  uint32_t seed = 1;
};

// One frame's effects for the world pass.
struct FxFrame {
  effects::Particles births;
  std::vector<effects::Light> lights;
  double blend[4] = {0, 0, 0, 0};  // the screen's: sRGB colour and share
  // The same without a lava's or a slime's tint, for a look that draws the liquid round the eye.
  double blend_dry[4] = {0, 0, 0, 0};
  std::vector<Placed> models;  // the entities with a model (world_models.hpp)
  // The weapon in hand's muzzle fire (app/muzzle.hpp): its shots' smoke left in the world and
  // their flames on this frame's muzzle, for the explosions' pass.
  std::vector<ge::render::Blast> puffs;
  std::vector<ge::render::MuzzleFire> flames;
};

// The effects of the wall frame of `dt` s at time `t` s, after frame_view brought `v` up to date.
void step_fx(const Listen& l, const ViewState& v, FxState& s, double t, double dt, FxFrame& out);

}  // namespace shooters_edge::client
