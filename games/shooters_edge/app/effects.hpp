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
// - LIGHTS: the flash at a muzzle, a rocket's glow, an explosion's fading light.
// - THE SCREEN'S BLEND: the liquid round the eye, a hurt's red flash fading, and the powers held,
//   mixed into one colour and share.
//
// ge:summary Shooter's Edge's effects: trails, bursts, dynamic lights and the screen's blend
// ge:keywords effects particles trail rocket grenade explosion spark smoke blood teleport lava
// ge:keywords splash muzzle-flash dynamic-light screen-blend damage-flash powerup underwater
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "brush/app/bsp_draw.hpp"
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

// The flash at a muzzle (one frame's), a rocket's glow, and an explosion's light `age` s after it
// (nothing past kExplosionLight s).
inline constexpr double kExplosionLight = 0.5;
Light muzzle_light(const double at[3]);
Light rocket_light(const double at[3]);
bool explosion_light(const double at[3], double age, Light* out);

// THE SCREEN'S BLEND: what surrounds the eye, how hurt the player is now, the powers held.
enum class Medium : uint8_t { air, water, slime, lava };
struct Screen {
  Medium medium = Medium::air;
  double hurt = 0;  // the hurt's flash now: a hit's damage, falling by kHurtFade a second
  bool power = false, shield = false, suit = false, stealth = false;
};
inline constexpr double kHurtFade = 150;  // damage a second the flash fades by
// The hurt's flash after `dt` s more (the next frame's `hurt`), and a hit of `damage` on it.
double fade_hurt(double hurt, double dt);
double take_hurt(double hurt, double damage);
// The one colour (sRGB-encoded) and share the post pass mixes the picture toward.
void screen_blend(const Screen& s, double rgba[4]);

}  // namespace shooters_edge::effects
