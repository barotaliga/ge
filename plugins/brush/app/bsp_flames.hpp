// THE MAP'S FIRES (docs/rendering.md § Flames): every light entity that stands for a fire (a wall
// torch, a large flame) drawn as burning gas (render/flame.hpp) where its model's flame is, in
// place of the model. The gas keeps step with the light the map casts for it: its level is the
// light's `flicker_at`, so the fire and the wall it lights waver as one.
//
// BONFIRES (`FireLook::bonfires`, classic-enhance's ce_bonfires): a large flame standing on the
// floor with room over it (a brazier's bowl, `place_fires`) burns as a bonfire rather than a big
// torch: a broad 2 m body with four lesser tongues round it, a brighter light from higher up, and
// sparks thrown up out of its bed (render/emitters.hpp's sparks, `bonfire_sparks`). One with no
// such room (a LANTERN: a cage round it, or a low roof) burns bigger than the pack's flame and
// smaller than a bonfire, filling its cage, with no sparks. A wall torch stays a torch. The flames
// lean and the sparks drift in the air at the fire: the map's wind (`FireLook::air`) as far as the
// sky is open over it (`PointLight::sky`), and a slow draught of its own, so a fire in a closed
// room still wavers.
//
// ge:summary A map's torches and large flames as render::Flame gas synced to their lights; bonfires
// ge:keywords brush bsp torch flame fire wall-torch light-entity flicker-sync bsp_fires bonfire
// ge:keywords brazier sparks bonfire_sparks place_fires fire_air draught wind ce_bonfires lantern
// ge:skill brush render
// ge:test tests/brush_bsp_flames_test.cpp
#pragma once

#include <vector>

#include "brush/app/draw_data.hpp"
#include "render/flame.hpp"
#include "render/particles.hpp"

namespace ge::brush::app {

// A model's flame over its entity's origin, in map units (1 unit is kUnitM): where its gas leaves
// the fuel and how tall it stands. A wall torch's stick reaches about 6 units up and its flame
// to 29; a large flame's bowl about 2 and its flame to 43; a bonfire on that bowl to 82.
inline constexpr double kTorchBase = 6, kTorchHeight = 23;
inline constexpr double kLargeBase = 2, kLargeHeight = 41;
inline constexpr double kBonfireHeight = 80;
// m: how far from a bonfire's middle its lesser tongues burn, round the bowl.
inline constexpr double kBonfireBed = 0.28;
// How much brighter a bonfire's light is than the large flame's the map was lit for.
inline constexpr double kBonfireLight = 1.6;
// A lantern's flame under bonfires: its height, units (a large flame's 41, a bonfire's 80), how
// many times a large flame's width it burns, and how much brighter its light is.
inline constexpr double kLanternHeight = 58, kLanternWidth = 1.5, kLanternLight = 1.25;
// A bonfire's sparks a second.
inline constexpr double kBonfireSparks = 40;

// How the fires burn this frame.
struct FireLook {
  bool bonfires = false;      // a large flame burns as a bonfire, with sparks
  double air[3] = {0, 0, 0};  // m/s: the wind over the open map now (bsp_weather's wind_air)
  double draught = 0.35;      // m/s: the slow draught every fire wavers in, open sky or not
};

// Whether a light's fire burns as a bonfire under `look`.
bool is_bonfire(const PointLight& light, const FireLook& look);

// A fire's flame's height in map units under `bonfires` or not (0 for a light that is no fire).
double fire_height(const PointLight& light, bool bonfires = false);
// How far above its entity's origin a fire's light shines from, m: from the middle of its gas
// (where render::flame_light puts a flame's), 0 for a light that is no fire.
double fire_light_lift(const PointLight& light, bool bonfires = false);
// A fire's light softened over its flame's height, m (0 for a light that is no fire).
double fire_light_size(const PointLight& light, bool bonfires = false);
// How many times the map's light a fire's light is under `bonfires` (a bonfire's, a lantern's).
double fire_light_gain(const PointLight& light, bool bonfires = false);

// How bright a fire's core is against a lightmap's 1 (a white face as the map's light lights it).
inline constexpr double kFireGlow = 30;

// m: the room a bonfire needs over its bed and round it, free of the map's walls (a lantern's cage,
// a low roof); a large flame without it burns as a plain flame.
inline constexpr double kBonfireRoom = 2.2, kBonfireReach = 0.6;

// Each light's fire placed in the map, once a map (draw_data's scene does it): the share of open
// sky over it (`PointLight::sky`, its columns looked up the map's tree), and a large flame without
// a bonfire's room over and round it made a `Fire::lantern`.
void place_fires(Scene& scene);

// The air at light `light`'s fire at `t_s`, m/s: the wind times its open sky and its own draught.
void fire_air(const PointLight& light, const FireLook& look, double t_s, double out[3]);

// The scene's fires at `t_s` (into `out`, cleared first), each seeded by its light; `brightness`
// is the renderer's (Options::brightness), what a lightmap's 1 is against the open day's white.
void bsp_fires(const Scene& scene, double t_s, double brightness, std::vector<render::Flame>& out,
               const FireLook& look = {});

// The sparks the scene's bonfires throw in [t0_s, t1_s), each flown to t1_s, appended to `out`
// (none unless `look.bonfires`).
void bonfire_sparks(const Scene& scene, double t0_s, double t1_s, const FireLook& look,
                    std::vector<render::Particle>& out);

}  // namespace ge::brush::app
