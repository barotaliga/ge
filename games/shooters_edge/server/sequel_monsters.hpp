// THE SEQUEL'S MONSTERS: the single player's enemies, run by the sequel runner
// (sequel_server.hpp) from the clean-room note porting/shooters-edge/sequel-monsters.md (the rules
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
// - **Attack**: at melee range its melee (at skill 0 three frames in four nothing), else a chance a
//   frame of 0.4 holding its ground, 0.2 melee, 0.1 near, 0.02 mid (halved at skill 0, doubled at
//   2 and 3) once its last attack's wait (2 × random s) is spent; it follows the last sighting and
//   gives up 20 s after it last saw its enemy. Pain at most every 3 s (5 s at skill 3).
// - **The full game's** (sequel-monsters-full.md, the roster in sequel_monster_kinds.hpp): the
// medic
//   raises the corpse it sees with the most health; a power screen; bosses with their own attack
//   test that burn down to a wreck; Jorg's death brings out the Makron; swimmers; two figures.
//
// ge:summary The sequel's monsters: roster, senses, choosing to attack, the step move, pain, death
// ge:keywords sequel monsters monster-ai senses sight-entity noise ambush path-corner point-combat
// ge:keywords soldier infantry gunner berserk parasite flyer tank step-move corpse gib
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_monsters_test.cpp
#pragma once

#include "shooter/sequel_move.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;

struct SequelServer;

// The monsters' models and sounds into the lists (before the player's model, which stays last).
void monster_precache(SequelServer& s);
// The map's monsters set up (after the movers): removed in deathmatch, dropped to the floor.
void monster_spawn(SequelServer& s);
// One rules frame of every monster: notice, chase, attack, move.
void monster_frame(SequelServer& s);
// A blow of `damage` from `attacker` (a slot, or -1) to map entity `e` along `dir`. False when `e`
// is no living monster.
bool monster_damage(SequelServer& s, int e, int attacker, int damage, sh::Vec3f dir);
// A use of map entity `e` (a monster waiting hidden appears). False when `e` is no monster.
bool monster_use(SequelServer& s, int e, int activator);
// Player `slot` made a noise a monster may hear (his weapon).
void monster_noise(SequelServer& s, int slot);
// The monsters' world rows: place, facing, model, skin.
void monster_rows(SequelServer& s);
// The monster of map entity `e` (its index in `SequelServer::monsters`), or -1.
int monster_index(const SequelServer& s, int e);
// The words of a death by a monster's attack (`cause` from `kSqByMonster`), or null.
const char* monster_obituary(int cause);

}  // namespace shooters_edge::server
