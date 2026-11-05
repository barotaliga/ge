// SHOOTER'S EDGE'S EFFECTS (docs/shooters-edge.md § The picture): what the game throws into the
// air and flashes on the walls and the screen when something happens, on the engine's particles
// (render/particles.hpp), the world's dynamic lights (brush/app/bsp_draw.hpp) and the post pass's
// screen blend (render/post.hpp's `set_post_blend`). Each effect is a pure function of where it
// happens and a seed, so a replay throws the same sparks; every number is our own, in metres,
// seconds and kelvin.
//
// - TRAILS: a rocket's (embers that cool from yellow to dark red, and grey smoke that rises) and
//   a grenade's (smoke alone), laid along the stretch it flew this frame.
// - BURSTS: an explosion (a ball of sparks, smoke, a light that fades), a hit on a wall (sparks off
//   the face and a puff of dust), a wound (dark droplets that fall), a teleport's arrival (a column
//   of cold light that spreads), a splash of lava (glowing globs thrown up).
// - DUST: a figure leaving (a teleport's near end) crumbles into ash that the wind takes, the
// flakes
//   thrown off where its dissolve's cut passes each frame; one arriving (a spawn, a teleport's far
//   end) is blown in as ash from upwind, each flake landing where the body appears as it appears.
//   The figure's own dissolve is the morph pass's (render/morph.hpp's MorphWarp); both read the
//   same field (`warp_field`), so the flakes leave and land on the cut.
// - LIGHTS: the flash at a muzzle, a rocket's glow, an explosion's fading light.
// - THE SCREEN'S BLEND: the liquid round the eye, a hurt's red flash fading, and the powers held,
//   mixed into one colour and share.
//
// ge:summary Shooter's Edge's effects: trails, bursts, dynamic lights and the screen's blend
// ge:keywords effects particles trail rocket grenade explosion spark smoke blood teleport lava
// ge:keywords splash muzzle-flash dynamic-light screen-blend damage-flash powerup underwater
// ge:keywords warp dust dust_off dust_in ash flakes disintegrate spawn teleport respawn materialize
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "render/morph.hpp"
#include "render/particles.hpp"

namespace shooters_edge::effects {

using Particles = std::vector<ge::render::Particle>;
using Light = ge::brush::app::DynamicLight;

// A rocket's trail over the stretch `from` .. `to` (engine metres): an ember and a puff of smoke
// every kTrailStep m.
inline constexpr double kTrailStep = 0.12;
void rocket_trail(const double from[3], const double to[3], uint32_t seed, Particles* out);
// A grenade's: smoke alone, thinner.
void grenade_trail(const double from[3], const double to[3], uint32_t seed, Particles* out);

// An explosion at `at`: sparks thrown out in a ball and smoke.
void explosion(const double at[3], uint32_t seed, Particles* out);
// A shot's hit on a face whose normal (unit) is `normal`: sparks off it and a puff of dust.
void impact(const double at[3], const double normal[3], uint32_t seed, Particles* out);
// A wound at `at`, the shot going `dir` (unit): dark droplets that fall.
void blood(const double at[3], const double dir[3], uint32_t seed, Particles* out);
// A teleport's arrival at `at` (the feet): a column of cold light that spreads and fades.
void teleport_splash(const double at[3], uint32_t seed, Particles* out);
// Lava thrown up at `at` (its surface): glowing globs that arc and fall.
void lava_splash(const double at[3], uint32_t seed, Particles* out);

// A figure turned to dust or made of it: where it stands, how its model is turned, its warp's
// sweep (axis, span, seed: the share is the caller's), the wind and its body.
struct Dust {
  double at[3] = {0, 0, 0};          // its position: the morph draw's (engine m)
  float rotation[4] = {0, 0, 0, 1};  // its model's frame to the world's, as drawn
  ge::render::MorphWarp warp;
  double wind[3] = {0, 0, -1};   // downwind, unit
  double facing[3] = {0, 0, 1};  // the way it faces, unit, level
  // Its skin: nine floats a triangle in its model's own frame (m), as drawn this frame. Empty: an
  // upright ellipse `half_width` across and `half_depth` deep from `low` to `high` m above `at`.
  std::span<const float> skin;
  double half_width = 0.22, half_depth = 0.14, low = -0.6, high = 0.7;
  // Seen from inside (the figure's own eyes): the ash lands in the air ahead of `eye` instead of
  // on the skin, and no flake flies within `eye_clear` m of it.
  bool keep_off_eye = false;
  double eye[3] = {0, 0, 0}, eye_clear = 0.45;
};
// The ash of a figure leaving over a frame, as the share fell from `from` to `to`: a flake off
// each point of its skin the cut passed, each on a way of its own (a wide cone round the wind,
// some lifted, some sinking, each slowed and held up by the air its own amount).
void dust_off(const Dust& d, double from, double to, uint32_t seed, Particles* out);
// The ash of a figure arriving over a frame, from `was` to `age` s into it (`was` < 0: its first):
// each flake flies its own way and time (0.1 to 0.5 s) to a point of the skin and ends there as
// that point appears, `land_s(share)` s in; the ones that must set off in this frame are born.
void dust_in(const Dust& d, double was, double age, double (*land_s)(double share), uint32_t seed,
             Particles* out);

// The flash at a muzzle (one frame's), a rocket's glow, and an explosion's light `age` s after it
// (nothing past kExplosionLight s).
inline constexpr double kExplosionLight = 0.5;
Light muzzle_light(const double at[3]);
Light rocket_light(const double at[3]);
bool explosion_light(const double at[3], double age, Light* out);

// THE SCREEN'S BLEND: what surrounds the eye, how hurt the player is now, the powers held.
enum class Medium : uint8_t { air, water, slime, lava };
struct Screen {
  double hurt = 0;    // the hurt's flash now: a hit's damage, falling by kHurtFade a second
  double armour = 0;  // the same for what the armour took of it: a duller amber under the red
  Medium medium = Medium::air;
  bool power = false, shield = false, suit = false, stealth = false;
};
inline constexpr double kHurtFade = 150;  // damage a second the flash fades by
// The hurt's flash after `dt` s more (the next frame's `hurt`), and a hit of `damage` on it.
double fade_hurt(double hurt, double dt);
double take_hurt(double hurt, double damage);
// The one colour (sRGB-encoded) and share the post pass mixes the picture toward.
void screen_blend(const Screen& s, double rgba[4]);

}  // namespace shooters_edge::effects
