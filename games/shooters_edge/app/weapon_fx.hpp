// SHOOTER'S EDGE'S ENHANCED WEAPON EFFECTS (docs/shooters-edge.md § The picture): what the modern
// look draws for a weapon's work under the classic-enhance switches, on the engine's effects (the
// fireball and its smoke, render/explosion.hpp; the bolt, render/beam.hpp; a round's hit,
// render/impact.hpp; the particle pool, render/particles.hpp). The classic game's character is
// kept: a blast is still a ball of orange embers flung out and falling, the rocket still trails
// fire into grey smoke, the arc gun's bolt is still a jagged blue-white line struck again many
// times a second, a pellet still kicks grit off the wall. The engine adds what the classic could
// not draw: a fireball of hot gas that swells, rolls and turns to smoke, its light white-hot then
// orange, smoke that spreads as it thins, a burst that answers what was hit.
//
// - THE BLAST (`blast`): a LAYERED fireball, as a film's explosion is: the main ball, then two or
//   three smaller billows born a few hundredths of a second later round it (a round ball reads as a
//   sprite; lobes read as gas), the classic's EMBER BALL (two kinds: fast ones flung out that fall,
//   and slow ones that hang and drift, each cooling from yellow through orange to a dull red), a
//   spray of white-hot sparks streaked by their speed, a few dark chunks, and smoke that lingers.
//   Its LIGHT (`blast_light`) follows the main ball's flash: white-hot for a moment, then an orange
//   glow that falls away, reaching as far as the classic's.
// - THE ROCKET (`exhaust`, `fire_trail`): a short hot flame out of its motor each frame (the
//   muzzle fire's march, stretched back along its flight), embers shed behind it that cool fast,
//   and puffs of warm grey smoke laid every few centimetres that rise and spread. Its motor's fire
//   lights the world round it as it flies (`motor_light`): a warm flame's colour at the nozzle, a
//   little behind the rocket, wavering as a flame does, where the classic rocket's light is a
//   steady white one on it. A GRENADE trails thinner, paler smoke alone (`smoke_trail`).
// - THE BOLT (`bolt_look`): one look, the same for every player: render/beam.hpp's lightning
//   channel as thin as the classic's bolt (a hair-thin white-hot core in a narrow blue sheath,
//   kinked every few centimetres and wandering a hand's width off its line, struck afresh thirty
//   times a second with short crackling forks), its brightness keyed to the eye so it reads the
//   same by day and in a dark room at night. It is the brightest light in the room
//   (`bolt_lights`): a hard blue-white light at the gun, along the beam and where it lands,
//   flickering with every strike and reaching as far as the classic's. Where it lands, blue-white
//   sparks (`bolt_hit`) and a scorch.
// - THE HITS (`hit`): a pellet's or a nail's hit TEARS DEBRIS off what it hit, and leaves no smoke
//   or dust in the air: a few big chunks and more small chips of the face, and a spray of dirt,
//   all in the wall's own colour, flung out of the hole and falling on their arcs, the pieces
//   hopping on the floor below before they lie still; how much and how fast is the surface's
//   (`impact_look_for` with engine/detail's surface word): sand sprays dirt alone, metal sheds
//   little, water splashes. A nail off metal strikes sparks. The hole is the client's mark.
//
// Each is a pure function of where, when and a seed (ge::core::Rng, the engine's pure looks), so a
// replay throws the same fire. Every number is our own, in metres, seconds and kelvin.
//
// ge:summary Shooter's Edge's enhanced weapon effects: layered blasts, exhaust, the bolt, the hits
// ge:keywords weapon-fx enhanced effects explosion fireball layered secondary embers ember-ball
// ge:keywords rocket-trail exhaust grenade-trail fire_trail smoke_trail smoke lightning bolt
// ge:keywords beam arc-gun impact hit
// ge:keywords pellet nail surface classic-soul blast_light debris chunks dirt no-smoke
// ge:skill render shooter
// ge:test tests/shooters_edge_weapon_fx_test.cpp
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "render/beam.hpp"
#include "render/explosion.hpp"
#include "render/explosions.hpp"
#include "render/muzzle.hpp"
#include "render/particles.hpp"
#include "shooters_edge/app/effects.hpp"

namespace shooters_edge::effects {

namespace gr = ge::render;

// The looks (each made once; they outlive every blast and shot that points at them).
const gr::BlastLook& blast_look();     // the main fireball
const gr::BlastLook& billow_look();    // a later, smaller billow round it
const gr::MuzzleLook& exhaust_look();  // a rocket's motor
// The arc gun's bolt: the one look every player sees.
const gr::BeamLook& bolt_look();
// The same bolt across the gun's prongs while it fires: finer (kinked every 3 cm over the
// prongs' 30 cm) and thinner, as it is a hand's breadth from the eye.
const gr::BeamLook& prong_arc_look();

// Billows round a blast's main ball, and when the last is born after it, s.
inline constexpr int kBillows = 3;
inline constexpr double kBillowsBy = 0.12;

// An explosion at `at` (world m) at `now_s`: its fireball and its billows into `blasts` (each
// drawn while blast_life says), its embers, sparks and chunks into `births` (once).
// `normal` (unit, out of it) the surface it went off against, or null in the open air: the ball
// then stands a third of its radius off it, a dome rather than a ball half in the wall, and throws
// into the half space it faces.
void blast(const double at[3], const double* normal, uint32_t seed, double now_s,
           std::vector<gr::Blast>* blasts, Particles* births);
// The light of the main fireball `b` at `now_s`: false once its glow is gone.
bool blast_light(const gr::Blast& b, double now_s, Light* out);

// THE LAUNCH AS IT LOOKS (visual only: the server's rocket flies its straight line from the
// muzzle at full speed). A rocket pops out of the tube slowly and drops a little, its motor lights
// kIgniteS later, and it speeds up to catch its true place by kLaunchS; on the way the shooter's
// own sway carries it off the line a moment (his movement across the line, his aim's swing: it
// lags behind a turn) and it wavers a little off its own seed, all of it settling onto the line.
// `launch_offset` is the offset from its true place `age_s` after it left, world m.
inline constexpr double kIgniteS = 0.05, kLaunchS = 0.5;
struct RocketLaunch {
  double kick[3] = {0, 0, 0};  // m: the shooter's sway's push off the line at its most
  uint32_t seed = 0;
};
void launch_offset(double age_s, const double dir[3], const RocketLaunch& launch, double out[3]);
// The push of a shooter moving at `velocity` (world m/s) with his aim turning at `yaw_rate` and
// `pitch_rate` (rad/s: right and up positive) when he fired along `dir` (unit); capped at kKickM.
inline constexpr double kKickM = 0.6;
void launch_kick(const double dir[3], const double velocity[3], double yaw_rate, double pitch_rate,
                 double out[3]);

// A rocket's motor flame at `at` going `dir` (unit) this frame.
gr::MuzzleFire exhaust(const double at[3], const double dir[3], uint32_t seed, double now_s);
// The light of that flame at `now_s`: at the nozzle, kNozzleM behind `at` along `dir` (a zero
// `dir`, on it), the flame's orange-yellow, reaching kMotorLightM, its level wavering at a flame's
// 8 to 12 Hz between 0.8 and 1 of kMotorLevel, off the rocket's own seed so two never waver as one.
inline constexpr double kNozzleM = 0.3, kMotorLightM = 6.5, kMotorLevel = 1.7;
Light motor_light(const double at[3], const double dir[3], uint32_t seed, double now_s);
// A grenade's painted ends glowing (ce_trails; a classic grenade gives no light): `rgb` the colour
// its skin's glowing texels paint (world_models' ModelKind::glow_rgb), at kGrenadeGlowLevel,
// reaching kGrenadeGlowM, a faint red on the floor and the wall it passes; it rides on `entity`,
// so the grenade's own body is not lit by it and only its ends show the glow.
inline constexpr double kGrenadeGlowM = 1.6, kGrenadeGlowLevel = 0.45;
Light grenade_glow(const double at[3], const float rgb[3], int entity);
// The trails over the stretch `from` .. `to` (m) flown this frame.
inline constexpr double kRocketStep = 0.06, kGrenadeStep = 0.08;
void fire_trail(const double from[3], const double to[3], uint32_t seed, Particles* out);
void smoke_trail(const double from[3], const double to[3], uint32_t seed, Particles* out);

// A round's hit on a wall: where (world m), the face's normal (unit, out of it) and the way the
// round went (unit), the face's surface class (engine/detail's word: "stone", "metal", ...) and
// its texture's colour (linear; all below 0: the class's own shade), and the floor under the hit
// (world y, m) that what it knocks loose comes down on.
struct WallHit {
  double at[3] = {0, 0, 0};
  double normal[3] = {0, 1, 0};
  double dir[3] = {0, 0, 1};
  std::string_view surface = "stone";
  float colour[3] = {-1, -1, -1};
  double floor_y = -1e9;
  // The map's baked light on the face where it hit (its lightmap there, 1 a channel as painted);
  // all below 0: not known, the debris lit as any particle is (the lamps and the sky).
  float light[3] = {-1, -1, -1};
  bool nail = false;  // a nail's (more sparks, a deeper bite), else a pellet's
};
// THE HIT, in the WALL'S COLOUR and with NO SMOKE: debris torn out of the hole along the burst's
// axis (the face's normal leaning toward the round's glance): CHUNKS (two or three, 2.4 to 3.6
// times a chip of the surface across, slow, hopping on the floor and lying a few seconds), CHIPS
// (the surface's count, smaller and faster) and DIRT (fine grains, the fastest, raining down); all
// in the wall's own colour, a little muted (a fresh break paler than the face) and, where the hit's
// baked light is known, shining in it as the wall does (unlit by the particles' lamps and sky,
// which indoors are a dim blue), all falling, none growing. Every solid face breaks: at least two
// chunks, six chips and twenty grains, more where the surface's look throws more. Of the engine's
// burst for the surface (render/impact.hpp) only a nail's sparks off metal and a splash's beads;
// its dust puff and its grit are not thrown. A liquid splashes and throws no debris; the sky,
// nothing.
void hit(const WallHit& h, uint32_t seed, Particles* births);

// The light of a nail's sparks off metal at `at` (world m), `age` s after the hit: a warm white
// flick a metre and a half round, gone in kSparkLightS; false once it is.
inline constexpr double kSparkLightS = 0.08;
bool spark_light(const double at[3], double age, Light* out);

// Where the bolt lands on `normal`'s face (unit): blue-white sparks.
void bolt_hit(const double at[3], const double normal[3], uint32_t seed, Particles* out);

// THE BOLT'S LIGHTS for `shot` at `t_s` into `out` (at most gr::kBeamLights; returns how many):
// render/beam.hpp's lights at the gun, along the beam and at the hit as the brush world's dynamic
// lights, each reaching kBoltReachM, at the beam's level this strike, and boosted by
// kBoltBoost over the other effects' lights so at the default r_fx_lights it lights the room
// brighter than the classic's bolt (a level of 1.6 at its middle) and more where it lands.
inline constexpr double kBoltReachM = 7;  // the classic's bolt reaches about 215 units
inline constexpr double kBoltBoost = 8;   // 1.6 / r_fx_lights' default
int bolt_lights(const gr::BeamShot& shot, double t_s, Light* out);

// THE SEQUEL'S SHOTS, by the kind of shot whoever fired it (a player or a monster):
//
// - a BLASTER'S BOLT: a hot yellow light riding it (kBoltGlowM) and a short trail of shining
//   yellow specks behind it (`glow_trail` with kBoltRgb);
// - the BFG'S BALL: a large green light (kBfgGlowM) and a thick trail of green specks;
// - the RAIL: a straight blue-white beam (`rail_look`, render/beam.hpp) wound by one filament's
//   helix, held a moment and fading over kRailFadeS; in the classic look a spiral of blue specks
//   round a white core (`rail_spiral`);
// - a bolt's hit: yellow sparks and a flick of light (`blaster_hit`); the BFG's burst: a green
//   ball of specks (`bfg_burst`) under the engine's blast.
inline constexpr float kBoltRgb[3] = {1.0f, 0.75f, 0.25f};
inline constexpr float kBfgRgb[3] = {0.35f, 1.0f, 0.35f};
inline constexpr double kBoltGlowM = 3.5, kBoltGlowLevel = 1.2;
inline constexpr double kBfgGlowM = 8, kBfgGlowLevel = 2.2;
inline constexpr double kRailFadeS = 1.0;
// The light riding a glowing shot at `at`: `rgb` at `level`, reaching `radius_m`, on `entity` (so
// the shot's own model is not lit by it).
Light shot_glow(const double at[3], const float rgb[3], double radius_m, double level, int entity);
// Shining specks of `rgb` over the stretch flown, `size` m across, one each `step` m.
void glow_trail(const double from[3], const double to[3], const float rgb[3], double size,
                double step, uint32_t seed, Particles* out);
const gr::BeamLook& rail_look();
void rail_spiral(const double from[3], const double to[3], uint32_t seed, Particles* out);
void blaster_hit(const double at[3], uint32_t seed, Particles* out);
void bfg_burst(const double at[3], uint32_t seed, Particles* out);

}  // namespace shooters_edge::effects
