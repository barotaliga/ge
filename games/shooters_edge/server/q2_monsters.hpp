// QUAKE II'S MONSTERS: the single player's enemies, run by Quake II runner
// (q2_server.hpp) from the clean-room note porting/shooters-edge/q2-monsters.md (the rules
// and numbers; the moves, speeds and timings between them are ours).
//
// - **Life**: removed in deathmatch; counted in the level's monsters; a walker drops to the floor
//   and walks by steps (up 18, never off a ledge), a flier flies; health, gib health, mass and box
//   by kind; an `item` dropped at death; targets fired at death with its enemy as the activator; a
//   corpse that shots pass, or gibs under its gib health. Spawn flags: 1 ambush, 2 waits hidden
//   for a use (then telefrags its spot and turns on the user), 4 sight (as ambush). A `target`
//   naming a `path_corner` is a path it walks; one naming a `point_combat` the spot it runs to on
//   first finding an enemy, and holds.
// - **Senses**: melee under 80, near under 500, mid under 1000; it sees a player not far, by a
// clear
//   line from eye to eye (25 above the origin), and in front (cos above 0.3) unless near and
//   already found. Each frame it notices the monster that saw a player (unless an ambusher), else a
//   noise a player made (his weapon) within 1000, else the frame's player in turn. Hurt by a
//   player, it turns on him.
// - **Infighting**: hurt by another monster that moves as it does (walks, flies or swims alike)
//   and is another kind (never one of the heavy kinds: tank, commander, super tank, Makron,
//   Jorg), or that was aiming at it, it turns on that monster (the player it fought kept, to go
//   back to once that fight is over); else it joins the attacker against its foe. A foe is a
//   player's slot or a monster as -2 - its entity (q2_server.hpp's `enemy`).
// - **Attack**: at melee range its melee (at skill 0 three frames in four nothing), else a chance a
//   frame of 0.4 holding its ground, 0.2 melee, 0.1 near, 0.02 mid (halved at skill 0, doubled at
//   2 and 3) once its last attack's wait (2 × random s) is spent; it follows the last sighting and
//   gives up 20 s after it last saw its enemy. Pain at most every 3 s (5 s at skill 3).
// - **The full game's** (q2-monsters-full.md, the roster in q2_monster_kinds.hpp): the
// medic
//   raises the corpse it sees with the most health; a power screen; bosses with their own attack
//   test that burn down to a wreck; Jorg's death brings out the Makron; swimmers; two figures.
//
// ge:summary Quake II's monsters: roster, senses, choosing to attack, the step move, pain, death
// ge:keywords q2 monsters monster-ai senses sight-entity noise ambush path-corner point-combat
// ge:keywords soldier infantry gunner berserk parasite flyer tank step-move corpse gib
// ge:keywords infighting monster-vs-monster old-enemy
// ge:skill shooter
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#pragma once

#include <string_view>

#include "classic/q2_move.hpp"
#include "shooters_edge/server/q2_pack.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;

struct Q2Server;

// The monsters' models and sounds into the lists (before the player's model, which stays last).
void monster_precache(Q2Server& s);
// The map's monsters set up (after the movers): removed in deathmatch, dropped to the floor.
void monster_spawn(Q2Server& s);
// One rules frame of every monster: notice, chase, attack, move.
void monster_frame(Q2Server& s);
// A player's projectile (a bolt, a rocket, the BFG's ball) fired from `start` whose line meets
// monster `e` first, `dist` away at `hit`, flying at `speed`: the monster may dodge it (duck), as
// its kind does (q2-monsters-full.md § Dodging and ducking).
void monster_dodge(Q2Server& s, int e, int shooter, float dist, float speed, sh::Vec3f hit);
// The frame a kind's duck holds low (its model's), or -1 for a kind that never ducks.
int monster_duck_frame(std::string_view classname);
// A blow of `damage` from `attacker` (a slot, or -1) to map entity `e` along `dir` (`splash` a
// blast's, which never takes the surprise's double; `point` where a shot landed, when one did).
// False when `e` is no living monster.
bool monster_damage(Q2Server& s, int e, int attacker, int damage, sh::Vec3f dir,
                    bool splash = false, const sh::Vec3f* point = nullptr);
// Whether map entity `e` is a monster lasers do not hurt (the hornet: BFG beams, laser traps).
bool monster_laser_proof(const Q2Server& s, int e);
// A use of map entity `e` (a monster waiting hidden appears). False when `e` is no monster.
bool monster_use(Q2Server& s, int e, int activator);
// Player `slot` made a noise a monster may hear (his weapon).
void monster_noise(Q2Server& s, int slot);
// A noise of player `slot`'s at `at`: his own (`impact` false: a jump, a shot) or his shot's
// impact, which only a monster with no enemy and no ambush hears.
void monster_noise(Q2Server& s, int slot, sh::Vec3f at, bool impact);
// The monsters' world rows: place, facing, model, skin.
void monster_rows(Q2Server& s);

// At spawn: a torso waiting unseen beside each Makron, shown when he falls whole
// (q2_monster_rows.cpp).
void monster_torsos(Q2Server& s);
// The monster of map entity `e` (its index in `Q2Server::monsters`), or -1.
int monster_index(const Q2Server& s, int e);
// Whether `classname` is a monster's (one of the roster's rows).
bool monster_class(std::string_view classname, Q2Pack pack = Q2Pack::base);
// A spare monster of a spawner (its entities' `block`, below -1) brought out at `at`, facing
// `angles`, thrown at `velocity`; whoever stands there dies, player or monster. It wakes as any
// monster does. False when no spare is left.
bool monster_bring_spare(Q2Server& s, int block, sh::Vec3f at, sh::Vec3f angles, sh::Vec3f velocity,
                         int activator);
// Whether map entity `e` is a monster that bleeds green (The Reckoning's gekk).
bool monster_green(const Q2Server& s, int e);
// Ground Zero's hidden brain that drives a turret_breach (q2_gz_monsters.cpp): set up, used
// (false when entity `i` is not one), and its frame.
bool gz_brain_spawn(Q2Server& s, int i);
// Ground Zero's `hint_path` set up as the map spawns (q2_gz_ai.cpp); false for any other entity.
bool gz_hint_spawn(Q2Server& s, int i);
bool gz_brain_use(Q2Server& s, int i, int activator);
// Ground Zero's tesla (q2_gz_tesla.cpp), missile `i`, hurt the monster at entity `e`: its field
// (`lo`..`hi`) a bad area for 30 s, and the monster turns on it when the area is new or it had no
// enemy, a medic leaving its patient; a player it fought is kept to go back to.
void gz_tesla_bad_area(Q2Server& s, int i, int e, sh::Vec3f lo, sh::Vec3f hi);
// Tesla `i` takes `damage`: at none left it bursts (q2_gz_weapons.cpp).
void gz_tesla_damage(Q2Server& s, int i, int damage);
void gz_brains(Q2Server& s);
// The words of a death by a monster's attack (`cause` from `kSqByMonster`), or null.
const char* monster_obituary(int cause);

}  // namespace shooters_edge::server
