// SHOOTER'S EDGE'S EFFECTS (docs/shooters-edge.md § The picture): what the game throws into the
// air and flashes on the walls and the screen when something happens, on the engine's particles
// (render/particles.hpp), the world's dynamic lights (brush/app/bsp_draw.hpp) and the post pass's
// screen blend (render/post.hpp's `set_post_blend`). Each effect is a pure function of where it
// happens and a seed, so a replay throws the same sparks; every number is our own, in metres,
// seconds and kelvin.
//
// THE CLASSIC EFFECTS (the classic look, and the modern look with an enhancement switched off):
// the classic client's own, in its numbers (map units over kUnitM), as unlit specks of the pack's
// own palette colours (render/particles.hpp's shining particles, `Palette` the pack's
// gfx/palette.lmp; none: greys), each colour run along its ramp as the classic does (a bright
// yellow to a dark brown, an orange to a grey) and falling at the classic's 40 units a second
// squared:
// - TRAILS: a rocket's (a speck every 3 units on the fire ramp, rising as it greys) and a
//   grenade's (the same from the ramp's greys), over the stretch it flew this frame.
// - BURSTS: an explosion (1024 specks off a 32-unit box at up to 256 units a second, half on the
//   bright ramp speeding out, half on the second ramp slowing), a shot's or a nail's hit (20 or 10
//   dark grey specks a round), a wound (20 dark red specks a drop), a teleport's arrival (a grid of
//   896 grey specks flying out), a splash of lava (1024 glowing specks thrown up).
// - LIGHTS: a muzzle's flash (5 m), a rocket's glow (5 m), an explosion's (8.75 m shrinking 7.5 m
//   a second for half a second): a white light on the lightmaps, as strong as the classic's.
// - THE SCREEN'S BLEND: the liquid round the eye, a hurt's red flash fading, and the powers held,
//   mixed into one colour and share.
//
// THE DUST (ce_warps, the modern look; off, a teleport's classic specks above): a figure leaving
// (a teleport's near end) crumbles into ash that the wind takes, the flakes thrown off where its
// dissolve's cut passes each frame; one arriving (a spawn, a teleport's far end) is blown in as
// ash, each flake landing where the body appears as it appears. The figure's own dissolve is the
// morph pass's (render/morph.hpp's MorphWarp); both read the same field (`warp_field`), so the
// flakes leave and land on the cut.
//
// ge:summary Shooter's Edge's effects: trails, bursts, dynamic lights and the screen's blend
// ge:keywords effects particles trail rocket grenade explosion spark smoke blood teleport lava
// ge:keywords dynamic_light_at model-lighting rocket-glow explosion-light
// ge:keywords splash muzzle-flash dynamic-light screen-blend damage-flash powerup underwater
// ge:keywords palette ramp classic-specks shining
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
// The pack's palette, 256 RGB triples (gfx/palette.lmp); empty: each index a grey of its number.
using Palette = std::span<const uint8_t>;

// A palette entry as a shining speck's colour (linear rgb).
void palette_rgb(Palette palette, int index, float out[3]);

// A rocket's trail over the stretch `from` .. `to` (engine metres): a speck every kTrailStep m.
inline constexpr double kTrailStep = 3 * 0.025;
void rocket_trail(const double from[3], const double to[3], uint32_t seed, Palette palette,
                  Particles* out);
// A grenade's: the ramp's greys alone.
void grenade_trail(const double from[3], const double to[3], uint32_t seed, Palette palette,
                   Particles* out);

// An explosion at `at`: 1024 specks thrown out.
void explosion(const double at[3], uint32_t seed, Palette palette, Particles* out);
// A shot's or a nail's hit at `at`: `specks` dark grey specks (20 a pellet, 10 a nail, 20 a big
// nail's).
void impact(const double at[3], int specks, uint32_t seed, Palette palette, Particles* out);
// A wound at `at`: `specks` dark red specks (20 a drop; 50 the arc's, of the lava's reds).
void blood(const double at[3], int specks, bool arc, uint32_t seed, Palette palette,
           Particles* out);
// A teleport's arrival at `at` (the feet): a grid of grey specks flying out.
void teleport_splash(const double at[3], uint32_t seed, Palette palette, Particles* out);
// Lava thrown up at `at` (its surface): glowing specks over a 6 m square that arc and fall.
void lava_splash(const double at[3], uint32_t seed, Palette palette, Particles* out);

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

// THE DYNAMIC LIGHTS ON A MODEL: the light the frame's dynamic lights add at `w` (engine metres)
// in the floor probe's units (a luxel's byte over 255, what `light_under` reads), the classic's
// rule: each light's level falls in a straight line from its middle to nothing at its radius,
// and a light's level of 1 is a luxel of 128 (`DynamicLight::colour`), times `strength`.
// `toward` takes the unit way to where the added light comes from, each light weighted by its
// share (all 0 when none reaches).
void dynamic_light_at(std::span<const Light> lights, const double w[3], double strength,
                      double rgb[3], double toward[3]);

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
