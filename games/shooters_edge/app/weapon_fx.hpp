// SHOOTER'S EDGE'S ENHANCED WEAPON EFFECTS (docs/shooters-edge.md § The picture): what the modern
// look draws for a weapon's work under the classic-enhance switches, on the engine's effects (the
// fireball and its smoke, render/explosion.hpp; the bolt, render/beam.hpp; a round's hit,
// render/impact.hpp; the particle pool, render/particles.hpp). The classic game's character is
// kept: a blast is still a ball of orange embers flung out and falling, the rocket still trails
// fire into grey smoke, the arc gun's bolt is still a jagged blue-white line struck again many
// times a second, a pellet still kicks a puff of grit. The engine adds what the classic could not
// draw: a fireball of hot gas that swells, rolls and turns to smoke, its light white-hot then
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
//   and puffs of warm grey smoke laid every few centimetres that rise and spread. A GRENADE trails
//   thinner, paler smoke alone (`smoke_trail`).
// - THE BOLT (`bolt_look`): render/beam.hpp's lightning channel in the style the player picks
//   (the engine's jagged bolt by default, its plasma arc or its braid, or a finer-forked bolt),
//   its lights a cold blue-white glow that does not wash the room out.
//   Where it lands, blue-white sparks (`bolt_hit`).
// - THE HITS (`hit`): a pellet's or a nail's hit throws, subtly, the burst of what it hit
//   (`impact_look_for` with engine/detail's surface word): a small puff of dust, chips and grit in
//   the wall's own colour, flakes that fall and hop on the floor below, a thin wisp of smoke; a
//   nail strikes more sparks than a pellet. The hole it leaves is the client's mark.
//
// Each is a pure function of where, when and a seed (ge::core::Rng, the engine's pure looks), so a
// replay throws the same fire. Every number is our own, in metres, seconds and kelvin.
//
// ge:summary Shooter's Edge's enhanced weapon effects: layered blasts, exhaust, the bolt, the hits
// ge:keywords weapon-fx enhanced effects explosion fireball layered secondary embers ember-ball
// ge:keywords rocket-trail exhaust grenade-trail fire_trail smoke_trail smoke lightning bolt
// ge:keywords beam arc-gun impact hit
// ge:keywords pellet nail surface classic-soul blast_light
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
// The arc gun's bolt in each style a player picks by number (r_beam_style): 1 forked (finer
// kinks, wider forks, a bluer sheath), then the engine's three presets as it makes them
// (render/beam.hpp): 2 arc (a thick plasma arc), 3 bolt (jagged and forking, the default),
// 4 braid (a spinning rope of three); each with its light on the room turned down to a cold glow.
// A number out of range is the default's.
inline constexpr std::string_view kBoltStyles[] = {"forked", "arc", "bolt", "braid"};
inline constexpr int kDefaultBoltStyle = 3;
const gr::BeamLook& bolt_look(int style = kDefaultBoltStyle);

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

// A rocket's motor flame at `at` going `dir` (unit) this frame.
gr::MuzzleFire exhaust(const double at[3], const double dir[3], uint32_t seed, double now_s);
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
  bool nail = false;  // a nail's (more sparks, a deeper bite), else a pellet's
};
// THE HIT, kept subtle and MONOTONE: the engine's burst for its surface at about a round's own
// size (its puff of dust through the explosions' pass, a few sparks from a nail off metal only,
// chips and grit), all of it in the wall's own brightness in greys (a hint of its hue at most);
// FLAKES that peel off the face, fall and hop once or twice on the floor before they lie still; and
// one thin wisp of smoke that drifts up and spreads. Nothing for the sky.
void hit(const WallHit& h, uint32_t seed, double now_s, std::vector<gr::Blast>* blasts,
         Particles* births);

// Where the bolt lands on `normal`'s face (unit): blue-white sparks.
void bolt_hit(const double at[3], const double normal[3], uint32_t seed, Particles* out);

// A local light of the engine (a beam's) as the brush world's dynamic light.
Light dynamic_light(const gr::LocalLight& l);

}  // namespace shooters_edge::effects
