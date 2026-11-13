// THE MONSTERS' ATTACKS AS THE MODERN LOOK DRAWS THEM (docs/shooters-edge.md § The picture): what a
// classic game's monsters throw and spit, on the engine's effects (the particle pool,
// render/particles.hpp; the flame, render/muzzle.hpp; the fireball, render/explosion.hpp; the
// world's dynamic lights), under the classic-enhance switch `ce_monsters` in the modern look, the
// way the player's weapons are drawn by app/weapon_fx.hpp. Off, or in the classic look: the
// classic specks (`classic_tracer`, `classic_spit_hit`), as the classic client drew them.
//
// A monster's missile is known by its model: classic-enhance's replacement table names an effect
// `missile.<kind>` for each pack file (classicenhance/replace.hpp), and a model file's trail flags
// (the format's tracer bits) give the same kind to a mod's own model that carries them. The kinds:
//
// - ACID (`acid`): a flier's spit, a green-yellow glob. Classic: green specks either side of its
//   line. Modern: dripping glowing acid and a sickly green light round it; it splashes where it
//   lands, droplets running off the wall and a hissing green haze.
// - FLAME (`flame`): a knight's fire bolt. Classic: orange specks either side. Modern: a short
//   flame streaming back off it, embers shed, a flickering orange light; a burst of fire and embers
//   where it lands.
// - VOID (`void`): the big caster's homing ball. Classic: purple specks. Modern: a knot of violet
//   light with motes spiralling round its path and a dark wake; its own blast is the explosion's.
// - LAVA (`lava`): the boss's lava ball. Classic: the rocket's trail. Modern: the rocket's fire
//   and smoke trail, bigger, molten drops falling off it, a strong orange light.
// - LASER (`laser`): an enforcer's bolt. Classic: the model alone. Modern: a red-orange streak of
//   light that lights the corridor as it passes; sparks where it ends.
// - FLESH (`flesh`): what a zombie throws. Classic: a thin trail of blood specks. Modern: dark
//   drops that fall and fleck the floor.
//
// Each modern look was chosen from two variants filmed side by side (the `se-monster-fx` lab);
// the other was dropped. Every look is a pure
// function of where, when and a seed (ge::core::Rng), so a replay throws the same. Every number is
// our own, in metres, seconds and kelvin.
//
// ge:summary The monsters' missiles and spit in the modern look: acid, flame, void, lava, laser
// ge:keywords monster-fx monsters missile acid spit flier flame knight void-ball vore lava-ball
// ge:keywords boss laser enforcer flesh zombie tracer tracer-trail wizspike knightspike ce_monsters
// ge:keywords monster-attacks splash cable drain parasite medic heal monster-flash sequel
// ge:skill render shooter
// ge:test tests/shooters_edge_monster_fx_test.cpp
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "render/explosion.hpp"
#include "render/explosions.hpp"
#include "render/muzzle.hpp"
#include "render/particles.hpp"
#include "shooters_edge/app/effects.hpp"

namespace shooters_edge::effects {

enum class MonsterShot : uint8_t { none, acid, flame, void_ball, lava, laser, flesh };

// The kind an effect id of the replacement table names (`missile.acid`, ...), else none.
MonsterShot monster_shot_of(std::string_view effect);
// The kind a model file's trail flags give (the format's tracer bits 16, 64 and 128, the rocket's
// 1, the zombie's flesh 32), else none. The table's id wins where both say.
inline constexpr uint32_t kTrailRocket = 1, kTracerGreen = 16, kTracerOrange = 64,
                          kTracerPurple = 128;
MonsterShot monster_shot_of_flags(uint32_t flags);
const char* monster_shot_name(MonsterShot k);

// THE CLASSIC TRAILS over the stretch `from` .. `to` (m) flown this frame: a speck every 3 units,
// thrown either side of the line in turn and still where it was left, gone in half a second, in
// the pack's palette (green, orange, purple; the lava ball the rocket's fire, the flesh the
// zombie's blood). `count` carries the side from frame to frame (returned).
int classic_tracer(MonsterShot k, const double from[3], const double to[3], int count,
                   uint32_t seed, Palette palette, Particles* out);
// The classic splash where the spit lands (the temporary entity's codes 7 and 8): 30 brown-green
// specks, or 20 orange.
void classic_spit_hit(MonsterShot k, const double at[3], uint32_t seed, Palette palette,
                      Particles* out);

// THE MODERN LOOKS.
// The trail over the stretch `from` .. `to` (m) flown this frame.
void monster_trail(MonsterShot k, const double from[3], const double to[3], uint32_t seed,
                   Particles* out);
// Its glowing head this frame at `at` going `dir` at `speed` (m/s): a bright core and a soft halo
// round the pack's model, alive a few frames, so the shot reads across a room.
void monster_head(MonsterShot k, const double at[3], const double dir[3], double speed,
                  Particles* out);
// The light it carries at `at` going `dir` (unit; zero: still) at `now_s`; false for none.
bool monster_glow(MonsterShot k, const double at[3], const double dir[3], uint32_t seed,
                  double now_s, Light* out);
// The flame it carries this frame (the flame bolt's and the lava ball's), false for none.
bool monster_flame(MonsterShot k, const double at[3], const double dir[3], uint32_t seed,
                   double now_s, ge::render::MuzzleFire* out);
// A monster's gun fired (a soldier's shotgun, an enforcer's laser gun) at `at` pointing `dir`
// (unit), the shot at `born_s`: the muzzle fire of a shotgun, drawn kMonsterMuzzleS.
inline constexpr double kMonsterMuzzleS = 0.08;
ge::render::MuzzleFire monster_muzzle(const double at[3], const double dir[3], uint32_t seed,
                                      double born_s);
// Where it lands on the face `normal` (unit, out of it; null: in the air) coming along `dir`
// (unit): its burst into `births`, a fireball's puffs into `blasts`. The floor under it, `floor_y`.
void monster_splash(MonsterShot k, const double at[3], const double* normal, const double dir[3],
                    double floor_y, uint32_t seed, double now_s,
                    std::vector<ge::render::Blast>* blasts, Particles* births);
// The splash's light `age` s after it, false once gone.
inline constexpr double kSplashLightS = 0.35;
bool splash_light(MonsterShot k, const double at[3], double age, Light* out);

// THE SEQUEL'S MONSTER-ONLY ATTACKS (its shots themselves draw as the player's weapons', keyed by
// the shot's kind). A CABLE held between a monster and what it holds, sent each server frame:
// - DRAIN (`te_parasite_attack`): a parasite's tongue on its prey. Classic: a line of dull red
//   specks. Modern: a pulsing pink-red tongue that writhes along its length and spatters at the
//   end, with a red light where it bites.
// - HEAL (`te_medic_cable_attack`): a medic's cable on a corpse it raises. Classic: a line of
//   pale blue specks. Modern: a crackling blue-white cable that twists round its line, sparking
//   where it holds, with a cold light.
enum class Cable : uint8_t { none, drain, heal };
Cable cable_of(std::string_view effect);
// The cable from `from` to `to` (m) at `now_s`, its specks living until the next server frame.
void classic_cable(Cable c, const double from[3], const double to[3], uint32_t seed,
                   Particles* out);
void monster_cable(Cable c, const double from[3], const double to[3], uint32_t seed, double now_s,
                   Particles* out);
// Its light where it holds.
bool cable_light(Cable c, const double to[3], double now_s, Light* out);
// A MONSTER'S SHOT LEAVING (`te_monster_flash.<kind>`: blaster, gun, rocket, rail, bfg), at `at`
// going `dir` (unit): the flash's light and, in the modern look, its muzzle fire and sparks.
enum class Flash : uint8_t { none, blaster, gun, rocket, rail, bfg };
Flash flash_of(std::string_view effect);
bool flash_light(Flash f, const double at[3], double age, Light* out);
// The muzzle fire (the gun's and the rocket's), false for none; drawn kMonsterMuzzleS.
bool flash_fire(Flash f, const double at[3], const double dir[3], uint32_t seed, double born_s,
                ge::render::MuzzleFire* out);
// The sparks it throws once (the blaster's, the rail's and the BFG's coloured glow).
void flash_sparks(Flash f, const double at[3], const double dir[3], uint32_t seed, Particles* out);

// The fire bolt's burst as a small fireball.
const ge::render::BlastLook& flame_burst_look();

}  // namespace shooters_edge::effects
