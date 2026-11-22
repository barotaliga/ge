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
//   grenade's (the same from the ramp's greys), over the stretch it flew this frame; a gib's or a
//   head's blood (a dark red speck every 3 units, every 6 behind a zombie's flesh: still where it
//   was left, falling slowly, gone in 2 s), by the flag its model file carries.
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
// The kinds of light a setting can put out (`Light::group`): a rocket's or a missile's glow in
// flight, an explosion's, a power's glow round its carrier; the rest (a muzzle's flash, a bolt)
// none.
enum LightGroup : uint8_t {
  kLightOther = 0,
  kLightRocket = 1,
  kLightExplosion = 2,
  kLightPower = 3
};
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
// A wound at `at`: `specks` dark red specks (20 a drop; 50 the arc's, of the lava's reds), each
// thrown at `way` (engine m/s; none: they hang and fall).
void blood(const double at[3], int specks, bool arc, uint32_t seed, Palette palette, Particles* out,
           const double way[3] = nullptr);
// A model file's flags for the trail it leaves (the format's bits): a gib's blood, a zombie's
// flesh's thinner blood.
inline constexpr uint32_t kTrailGib = 4, kTrailZombieGib = 32;
// The blood a flying gib leaves over the stretch `from` .. `to` (engine metres): a speck every
// `step` m from `carry` m along (the stretch's last part short of a step carried into the next
// frame's, returned), so a gib leaves as much blood at any frame rate.
inline constexpr double kBloodStep = 3 * 0.025, kThinBloodStep = 6 * 0.025;
double blood_trail(const double from[3], const double to[3], double step, double carry,
                   uint32_t seed, Palette palette, Particles* out);
// A teleport's arrival at `at` (the feet): a grid of grey specks flying out.
void teleport_splash(const double at[3], uint32_t seed, Palette palette, Particles* out);
// QUAKE II'S ARRIVALS, in its own numbers (map units; its specks fall at twice the gravity they
// name, 80 units/s^2 for its 40): `at` is the entity's origin (a player's box's middle).
// - A teleport's burst (a player teleported or respawned, either end of a pad): a lattice of 1053
//   grey specks (palette 7..14) over 32 units across and 48 up from 16 below, each flying out at
//   50..113 units/s along its place with the two plan axes swapped, living 0.30..0.44 s.
// - An item come back: 64 green specks (212..215) in a 16-unit cube, drifting at up to 8 units/s,
//   falling at 16 units/s^2, living 1..1.3 s.
// - A player entering (`kSqLogin`, green 208..215), leaving (`kSqLogout`, red 64..71) or respawned
//   (`kSqRespawn`, yellow 224..231): 500 specks in a box 32 across from 24 below to 32 above,
//   drifting at up to 20 units/s, living 1..1.3 s.
// - A teleporter pad's fountain, each server frame: 8 specks (219) within 16 across and 1..8 below,
//   rising at 80..87 units/s, living 2 s.
enum Q2Arrival : uint8_t { kSqLogin = 0, kSqLogout = 1, kSqRespawn = 2 };
void q2_teleport_burst(const double at[3], uint32_t seed, Palette palette, Particles* out);
void q2_item_back(const double at[3], uint32_t seed, Palette palette, Particles* out);
// A burst of `count` specks of palette `colour` (each `colour` + 0..7 with `vary`) off a face along
// `normal` (map units' axes, x/y the plan; zero: none), each 0..`reach`-1 units out along it and
// -4..3 across, drifting at up to 20 units/s and falling, living 0.5..0.8 s: a laser's sparks
// (reach 8, one colour), a splash (reach 32, varied).
// `fall`: the specks' pull down in the trails' units (80 the usual; negative rises: a tunnel's
// sparks).
void q2_specks(const double at[3], const double normal[3], int count, int colour, int reach,
               bool vary, uint32_t seed, Palette palette, Particles* out, double fall = 80);
// QUAKE II'S OTHER EFFECTS OFF THE WIRE (map units, its own numbers):
// - BUBBLES along a line, the first at its start: grey (4..11), within 2 units, rising at 6
//   units/s and drifting at up to 5, a bubble every 32 units, living 1..1.2 s; the second kind
//   a bubble every 8 units, 4 lower, rising at 20 and drifting at up to 10, living 1..1.1 s.
// - A FORCE WALL: along its line every 4 units, seven steps in ten, a speck of its colour within
//   3 units, falling at 30..50 units/s, living 3..3.5 s.
// - STEAM (and the chain fist's smoke): `count` specks of `colour` + 0..7 within a tenth of the
//   jet's speed, along the jet (`jet`: its way times its speed) and spread a third of its speed
//   across it, pulled down by `fall` (40 steam, 0 smoke), living 0.5..0.8 s.
// - A BLUE BOLT'S HIT: 40 yellow-to-orange specks (224..231) 0..15 units out along `way`, flying
//   at 30 units/s along it and up to 40 any way, falling, living 0.5..0.8 s.
// - THE WIDOW'S SPLASH: 256 specks on a sphere of 45 units flying out at 40 units/s, of four
//   colours (16, 104, 168, 144), living 0.625..1 s.
void q2_bubbles(const double from[3], const double to[3], bool second, uint32_t seed,
                Palette palette, Particles* out);
void q2_force_wall(const double from[3], const double to[3], int colour, uint32_t seed,
                   Palette palette, Particles* out);
void q2_steam(const double at[3], const double jet[3], int count, int colour, double fall,
              uint32_t seed, Palette palette, Particles* out);
void q2_blue_hit(const double at[3], const double way[3], uint32_t seed, Palette palette,
                 Particles* out);
void q2_widow_splash(const double at[3], uint32_t seed, Palette palette, Particles* out);
void q2_player_flash(const double at[3], Q2Arrival kind, uint32_t seed, Palette palette,
                     Particles* out);
void q2_fountain(const double at[3], uint32_t seed, Palette palette, Particles* out);
// QUAKE II'S TRAILS, in its own numbers (map units; the specks' gravity as for its arrivals):
// - THE THINNING TRAIL (a gib's blood, a grenade's and a rocket's smoke): a try every half unit
//   along the stretch, a speck when a draw of 0..1023 falls under the piece's `count` (1024 when
//   it first shows), the count losing 5 a try down to 100: two specks a unit at first, one in
//   five after about 92 units. Its spread is fixed by the count at the frame's start: within 4
//   units and drifting at up to 15 units/s above 900, 2 and 10 above 800, else 1 and 5. Blood is
//   its dark reds (232..239), thrown down at 40 units/s with no gravity, living 1..1.4 s; smoke
//   its greys (4..11), rising at 40 units/s^2, living 1..1.2 s.
// - A rocket's fire beside its smoke: a speck one unit in eight, yellow to orange (220..223),
//   within 5 units, drifting at up to 20 units/s and falling, living 1..1.2 s.
// - A blaster's bolt: an orange speck (224) every 5 units, within 1, drifting at up to 5 units/s,
//   living 0.3..0.5 s.
// - A green gib's blood: the thinning trail in greens (219..226), as a gib's.
// - A FLAG's (and the tag token's) column: a speck of its colour (a red flag 242, a blue 115, the
//   tag 220) every 5 units within 16 of the line, drifting at up to 5 units/s, living 0.8..1 s.
// - An ION RIPPER's bolt: a speck every 5 units on the line (228..231), thrown 10 units/s one way
//   then the other along the world's x (`side` carries the swing from frame to frame), living
//   0.15..0.25 s.
enum class Q2Trail : uint8_t { blood, smoke, green_blood };
void q2_column_trail(const double from[3], const double to[3], int colour, uint32_t seed,
                     Palette palette, Particles* out);
// The trap's glow at `at` (its origin raised 32 units): a column of yellow specks rising from 14
// below to 50 above it, a speck every 5 units, and 8 thrown out from round it that fall.
void q2_trap(const double at[3], uint32_t seed, Palette palette, Particles* out);
// The disruptor's bolt: two black strands woven about its line, a speck every 3 units, rising
// slowly, 0.5 s each.
void q2_tracker_trail(const double from[3], const double to[3], uint32_t seed, Palette palette,
                      Particles* out);
// A tracker's dark shell: 300 black specks on a sphere of `radius_u` units round `at`, drawn for
// one frame.
void q2_tracker_shell(const double at[3], double radius_u, uint32_t seed, Palette palette,
                      Particles* out);
// A power screen's or shield's splash: 256 green specks on a sphere of `radius_u` units round `at`,
// moving out at 40 units a second, 0.5 to 0.8 s each.
void q2_power_splash(const double at[3], double radius_u, uint32_t seed, Palette palette,
                     Particles* out);
// The widow's beam-out, `ratio` (0..1) of its way through: 300 specks on a sphere of 45 × ratio
// units round `at` in its four colours, drawn for one frame.
void q2_beam_out(const double at[3], double ratio, uint32_t seed, Palette palette, Particles* out);
void q2_ion_trail(const double from[3], const double to[3], int& side, uint32_t seed,
                  Palette palette, Particles* out);
inline constexpr int kSqTrailStart = 1024, kSqTrailFloor = 100, kSqTrailFade = 5;
inline constexpr double kSqTrailAttemptUnits = 0.5;
void q2_trail(const double from[3], const double to[3], Q2Trail kind, int& count, uint32_t seed,
              Palette palette, Particles* out);
void q2_rocket_fire(const double from[3], const double to[3], uint32_t seed, Palette palette,
                    Particles* out);
void q2_bolt_trail(const double from[3], const double to[3], uint32_t seed, Palette palette,
                   Particles* out);
// FLIES round a corpse (its flies bit): black specks (palette 0) that swarm over a 60 s cycle
// from the first frame they are seen (`since_s` into it): their number rises from none to 162
// over 20 s, holds, and falls to none over the last 20 s, every second one of them drawn (81 at
// most). Fly i swings 64 units through `at` along the i-th of the 162 directions (sin(t + i), t
// the picture's clock in s) and 16 units out along a way that turns at its own two rates (0 to
// 2.55 radians a second). They are thrown afresh every frame, each living `life` s (the frame's).
inline constexpr int kSqFliesMax = 162;
inline constexpr double kSqFliesCycleS = 60;
int q2_flies_count(double since_s);
void q2_flies(const double at[3], double t, double since_s, double life, Palette palette,
              Particles* out);
// The light a player's entry (green), exit (red) or respawn (yellow) flash gives for the one
// frame it is drawn in: 200 to 231 units, by `seed`.
Light q2_arrival_light(const double at[3], Q2Arrival kind, uint32_t seed);
// A light of `radius_u` map units and colour (fractions 0..1) at `at`, as bright as its size (a
// welding spark's, a flashlight's).
Light q2_point_light(const double at[3], double radius_u, double red, double green, double blue);
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
// and a light's level of 1 is a luxel of 128 (`DynamicLight::colour`), times `strength` and
// its own `boost`.
// `toward` takes the unit way to where the added light comes from, each light weighted by its
// share (all 0 when none reaches). A light riding on entity `self` (its `entity`; 0: none) is
// left out: a model is not lit by its own glow.
void dynamic_light_at(std::span<const Light> lights, const double w[3], double strength,
                      double rgb[3], double toward[3], int self = 0);

// THE SCREEN'S BLEND: what surrounds the eye, how hurt the player is now, the powers held.
enum class Medium : uint8_t { air, water, slime, lava };
// THE CLASSIC TINTS (a single-player program's, as the original's client shows them): one
// powerup at a time, the first held of quad, suit, ring, pentagram.
enum class Powerup : uint8_t { none, quad, suit, ring, pent };
struct Screen {
  double hurt = 0;    // the hurt's flash now: a hit's damage, falling by kHurtFade a second
  double armour = 0;  // the same for what the armour took of it: a duller amber under the red
  double bonus = 0;   // an item taken: the gold flash's strength now, kBonusFlash falling to 0
  Medium medium = Medium::air;
  bool power = false, shield = false, suit = false, stealth = false;
  // The classic tints in place of the layers above: `hurt` is the damage tint's strength (of
  // 255) in `hurt_rgb` (0..255), `bonus` the pickup flash as above, `powerup` the one power's
  // tint. Combined contents, damage, bonus, powerup, each over the last.
  bool classic = false;
  Powerup powerup = Powerup::none;
  double hurt_rgb[3] = {255, 0, 0};
  // The tint over the view in open air (`v_cshift`, a mod's own): its colour (0..255) and its
  // strength (of 255); 0 none, the default.
  double air = 0;
  double air_rgb[3] = {130, 80, 50};
};
// The classic damage tint: a blow of `armour` and `blood` points adds 3 x its count (half of
// each, at least kHurtCountMin) to `strength`, kept at most kClassicHurtMost, and takes the
// blow's colour into `rgb`: (200, 100, 100) when the armour took more than the blood, (220, 50,
// 50) when it took some, else (255, 0, 0). The strength falls by kHurtFade a second.
inline constexpr double kHurtCountMin = 10, kClassicHurtMost = 150;
void take_classic_hurt(double& strength, double rgb[3], double armour, double blood);
inline constexpr double kHurtFade = 150;  // damage a second the flash fades by
// An item taken flashes the view gold: this strong at once (of 255, the share it lays over the
// picture), falling by kBonusFade a second, so it is gone in half a second.
inline constexpr double kBonusFlash = 50;
inline constexpr double kBonusFade = 100;
// The pickup flash after `dt` s more, and lit again by `taken` items since the last frame.
double fade_bonus(double bonus, double dt, int64_t taken);
// The hurt's flash after `dt` s more (the next frame's `hurt`), and a hit of `damage` on it.
double fade_hurt(double hurt, double dt);
double take_hurt(double hurt, double damage);
// How much of each layer the player lets through (the v_ cvars, app/look.hpp): the medium round
// the eye, the hurt's flash and the armour's, and each power's tint; 0..1 each, 1 the full.
struct BlendShares {
  double content = 1, damage = 1, quad = 1, pent = 1, suit = 1, ring = 1, bonus = 1;
};
// The one colour (sRGB-encoded) and share the post pass mixes the picture toward, each layer at
// its share of `k`.
void screen_blend(const Screen& s, double rgba[4], const BlendShares& k = {});

}  // namespace shooters_edge::effects
