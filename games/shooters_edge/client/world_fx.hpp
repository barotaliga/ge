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
// - A PLAYER'S WARP (warps.hpp): one leaving (a teleport's near end) is drawn where he stood,
//   crumbling from his back to his face as the ash the wind takes is thrown off where the cut
//   passes (app/effects.hpp's dust); one arriving (a spawn, a respawn, a teleport's far end) is
//   blown in as ash from ahead of him, each flake on a way of its own landing on his skin as it
//   appears there (FxState::models), whole after kArriveS. The local player's own arrival blows
//   the ash past his eyes (never through them) and assembles the weapon in his hands
//   (`held_shown`). That is the enhanced warp (`FxState::enhanced_warps`: ce_warps in the modern
//   look); off, the original: the figure there or gone at once and the teleport's fog at each end
//   (in a campaign the program's own; with the dust on, the program's fog at a player's warp is
//   left out).
// - THE SCREEN'S BLEND: the liquid round the eye (the player's `waterlevel` and `watertype`), and
//   a hit's flash, fading: in a campaign off the blows the program reported (Listen::blows: the
//   blood red, what the armour took a duller amber), in a match off the health lost.
//
// - THE HELD WEAPON'S FIRE: each shot's flame, smoke and sparks from the barrel the hand puts in
//   the world (app/muzzle.hpp, each weapon's row at the size its gun is drawn), under ce_muzzles.
//
// Every burst draws on its own seed from a counter, so the same run throws the same sparks.
//
// ge:summary The window's effects each frame: trails, explosions, their lights, the screen's blend
// ge:keywords world-fx effects trail rocket-trail explosion dynamic-light screen-blend hurt-flash
// ge:keywords underwater medium step_fx fx-state temp-entity gunshot spike teleport-splash beam
// ge:keywords held-fire muzzle-fire fire_held load_held_fire warp spawn-effect teleport-effect
// ge:keywords respawn ghost materialize dematerialize player_dust dust ash held_shown
// ge:skill render shooter
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "camera/subject.hpp"
#include "render/explosions.hpp"
#include "shooters_edge/app/effects.hpp"
#include "shooters_edge/app/muzzle.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/warps.hpp"
#include "shooters_edge/client/world_models.hpp"
#include "shooters_edge/view/hand.hpp"

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
  WarpWatch warps;  // the players' arrivals and departures (warps.hpp)
  // The enhanced warps (ce_warps in the modern look): the dust. Off: the original, the figure
  // there or gone at once and the teleport's fog at each end.
  bool enhanced_warps = true;
  std::unordered_map<int, Placed> seen;  // each player as drawn last frame, by entity
  struct Leaving {
    bool drawn = false;       // the figure was drawn (not the local player's own)
    Placed figure;            // where it stood
    std::vector<float> skin;  // its triangles as it stood (body_triangles)
    effects::Dust dust;       // its dust and its warp's sweep
  };
  std::unordered_map<int, Leaving> leaving;  // each departure, by entity
  std::vector<Body> bodies;                  // scratch: this frame's players
  std::vector<float> skin;                   // scratch: an arriving body's triangles
  // The models the window draws (world_draw.hpp's world_models), for the ash to land on their
  // skins; null: on each body's box.
  const WorldModels* models = nullptr;
  std::vector<Warp> began;  // scratch: the warps that began this frame
};

// One frame's effects for the world pass.
struct FxFrame {
  effects::Particles births;
  std::vector<effects::Light> lights;
  double blend[4] = {0, 0, 0, 0};  // the screen's: sRGB colour and share
  // The same without a lava's or a slime's tint, for a look that draws the liquid round the eye.
  double blend_dry[4] = {0, 0, 0, 0};
  std::vector<Placed> models;  // the entities with a model (world_models.hpp)
  float held_shown = 1;        // the weapon in hand's warp: the local player's arrival
  // The weapon in hand's muzzle fire (app/muzzle.hpp): its shots' smoke left in the world and
  // their flames on this frame's muzzle, for the explosions' pass.
  std::vector<ge::render::Blast> puffs;
  std::vector<ge::render::MuzzleFire> flames;
};

// A player's dust and the sweep of his warp from where he stands (`origin`, `angles`: map units
// and degrees) and his entity (the noise's seed): the wind blows into his face, so his back goes
// first and the ash streams behind him, and an arrival is blown in from ahead of him, his face and
// head first. The warp's share is the caller's.
effects::Dust player_dust(const sh::Vec3f& origin, const sh::Vec3f& angles, int entity);

// One frame of a warp's ash on `d`, `was` to `age` s into it (`was` < 0: its first frame): blown
// in to land on the body as it appears, or off it as the cut passes.
void warp_dust(const Warp& w, const effects::Dust& d, double was, double age, uint32_t seed,
               effects::Particles* out);

// The effects of the wall frame of `dt` s at time `t` s, after frame_view brought `v` up to date.
void step_fx(const Listen& l, const ViewState& v, FxState& s, double t, double dt, FxFrame& out);

// The held weapons' fire: every built weapon's look (by its index) and the shots still burning.
struct HeldFire {
  std::vector<ge::render::MuzzleLook> looks;
  effects::MuzzleShots shots;
};

// Each built weapon's fire from its row (view/weapons.hpp) at kBuiltScale; false with `error`
// naming the row when a row's knobs do not read.
bool load_held_fire(HeldFire& f, std::string* error);

// The hand's shot this frame (`hand.fired`) and the fire still burning, staged into `out` at time
// `now_s`; `on` false (ce_muzzles 0): a shot throws nothing, its light alone.
void fire_held(HeldFire& f, const view::HandFrame& hand, bool on, double now_s, FxFrame& out);

}  // namespace shooters_edge::client
