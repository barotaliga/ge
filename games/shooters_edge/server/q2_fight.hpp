// Quake II runner's fighting half (q2_server.hpp states the rules): the weapons' models and
// sounds listed at the map's start, the hand stepped and its shots made, missiles moved, damage
// dealt, deaths, gibs and obituaries, the weapon chosen, items dropped and taken back, and the
// world's rows for the shots in flight and the dropped items.
// Only q2_server.cpp calls these.
//
// ge:summary Quake II runner's fight: shots, missiles, damage, deaths, the chosen weapon
// ge:keywords q2 fight shots hitscan missiles radius-damage obituary death frags use-weapon
// ge:keywords hand-grenade drop-item gibs weapons-stay quad-drop teams friendly-fire
// ge:keywords corpse-queue scores fov
// ge:skill shooter
// ge:test tests/shooters_edge_q2_fight_test.cpp
#pragma once

#include <string>
#include <vector>

#include "shooters_edge/server/q2_gibs.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

// The weapons' view models and the missiles' models into the model list, their sounds into the
// sound list (before the player's model, which stays last).
void fight_precache(Q2Server& s);
// A fresh hand: the blaster, activating; the missiles of the map cleared (`all`).
void fight_spawn(Q2Server& s, int slot);
void fight_clear(Q2Server& s);

// The hand's step for this rules frame, with `attack` held: its shots made.
void fight_step(Q2Server& s, int slot, bool attack);
// Player `slot`'s hand stepped once (combat's `sq_hand_step`), with the one-shot sounds its new
// frame makes (q2_hand_sounds.cpp), and the sounds it may make.
SqStep fight_hand_step(Q2Server& s, int slot, bool attack, bool infinite, int swap);
extern const char* const kHandSounds[4];
// The missiles moved a rules frame (0.1 s): touches, fuses, explosions.
void fight_missiles(Q2Server& s);

// Weapon `w` chosen by a number key or `use`; ignored when not held.
void fight_choose(Q2Server& s, int slot, int w);
// A weapon taken from the floor (`item` its row): chosen at once when the blaster is in hand in
// deathmatch, always outside it.
void fight_took(Q2Server& s, int slot, int item);
// The player's `use`, `weapnext` and `weapprev` commands; false when `cmd` is none of them.
bool fight_command(Q2Server& s, int slot, const std::string& cmd, const std::string& arg);

// What killed a player when no weapon did (`fight_kill`'s `weapon`).
inline constexpr int kSqByFall = 100, kSqByLava = 101, kSqBySlime = 102, kSqByWater = 103;
// The hand grenade held to the end of its fuse.
inline constexpr int kSqByHeldGrenade = 104;
// A mover's crush, a hurting trigger, a teleport onto someone, a map's explosion.
inline constexpr int kSqByCrush = 105, kSqByHurt = 106, kSqByTelefrag = 107, kSqByExplosion = 108;
// A map's laser, an exploding barrel.
inline constexpr int kSqByLaser = 109, kSqByBarrel = 110;
// A monster's attack: this plus its row in the roster (q2_monsters.cpp).
inline constexpr int kSqByMonster = 200;

// How a blow landed (`Q2Server::blow_how`): straight on, a blast's splash, the BFG ball's laser
// on its way, the BFG's burst when it went off.
inline constexpr int kSqHowDirect = 0, kSqHowBlast = 1, kSqHowLaser = 2, kSqHowBurst = 3;

// The obituary line for player `slot` killed by `killer` (a slot; his own for himself, -1 none)
// with `weapon` (a weapon or a `kSqBy*`) landing as `how`: "<victim> <words>." for the world and
// himself, "<victim> <words> <killer><tail>" for another player, "<victim> died." for the rest.
// `gender` is the victim's `gender` setting (his, her or its); `by_killer` is set when the line
// credits the killer.
std::string sq_obituary(const std::string& victim, const std::string& killer, bool self, int weapon,
                        int how, const std::string& gender, bool* by_killer = nullptr);

// A blow to player `target` from `attacker` (a slot or -1) along `dir` (zero: no push), by
// `weapon` (a weapon or a `kSqBy*`); `flags` 1 passes armour by, 2 passes invulnerability by.
void fight_hurt(Q2Server& s, int target, int attacker, sh::Vec3f dir, int damage, int knockback,
                int weapon, int flags = 0);
// Radius damage at `at`: `damage` less half the distance, within `radius` (the attacker's half),
// to the players and the map's shootable movers it reaches.
void fight_blast(Q2Server& s, sh::Vec3f at, int attacker, int damage, float radius, int weapon);

// Midair (`k_midair` in deathmatch; plugins/shooter/midair.hpp): whether it is on; a spawn's kit
// (250 health, 200 body armour, the rocket launcher in hand, 255 rockets that never run out); the
// tally lines told at the level's end.
bool fight_midair(const Q2Server& s);
void fight_midair_kit(Q2Server& s, int slot);
std::vector<std::string> fight_midair_report(const Q2Server& s);

// Player `slot` dies, killed by `killer` (a slot; his own for a suicide, -1 the world) with weapon
// `weapon` (-1 none, or a `kSqBy*`); the obituary is scored and printed.
void fight_kill(Q2Server& s, int slot, int killer, int weapon);

// Item `item` (a weapon, an ammo or a powerup, by `drop <its name>`) thrown from player `slot`;
// false, with a line to him, when it cannot be.
bool fight_drop(Q2Server& s, int slot, int item);

// Item `item` used from the inventory (a powerup or power armour; `use <its name>`); false when
// it is not one or none is held.
bool fight_use(Q2Server& s, int slot, int item);

// Each frame: power armour switched on with no cells left switches off, with its sound
// (q2_use.cpp).
void fight_power_frame(Q2Server& s);
// Power armour's part of a blow on player `slot` along `dir` (a screen's only from within ~73
// degrees of his front: a third, a point a cell; a shield's two thirds, two points a cell), its
// cells taken; a part taken flashes it (`fight_power_hit`). The points it took (q2_use.cpp).
int fight_power_save(Q2Server& s, int slot, sh::Vec3f dir, int damage);
// A blow power armour took at `at` (its face along `normal`): 40 green specks off a screen, pale
// blue ones off a shield, and the hit's sound there; returns when its plate or shell stops showing.
double fight_power_hit(Q2Server& s, sh::Vec3f at, sh::Vec3f normal, bool shield);
// The end of player `slot`'s rules frame: the powerups' and the air's clocks, drowning, lava,
// slime and falling damage, and the body's sounds: the water's, breathing, landing, the frame's
// pain cry in the player's own voice (its `*` sounds, which each client takes from that
// player's model).
void fight_body_frame(Q2Server& s, int slot);
// Player `slot`'s stride came round (the view's bob clock passed a whole number this frame): a
// footstep when running on the ground faster than 225 units a second and no landing sounded.
void fight_stride(Q2Server& s, int slot);
// One command moved player `slot`: leaving the ground with the jump held (`up` 10 or more), out
// of the water, is the player's jump cry.
void fight_moved(Q2Server& s, int slot, bool was_on_ground, int up);
// The level's limits: true when the frag or time limit ended it (`next_map` named, the
// intermission due).
bool fight_limits(Q2Server& s);

// The items dropped (`drop`, or at death) falling to rest and vanishing when their time is up.
void fight_drops(Q2Server& s);
// The dropped items player `slot`'s box touches, taken by their rule.
void fight_touch_drops(Q2Server& s, int slot);
// Whether `dmflags` bit `bit` is set in deathmatch.
bool fight_dmflag(const Q2Server& s, int bit);

// The world's rows the fight writes: each live missile and dropped item, and every player's held
// weapon numbers.
void fight_rows(Q2Server& s);
// The first world row of the missiles; the dropped items' follow them.
size_t fight_first_row(const Q2Server& s);

// True when players `a` and `b` (two slots) are on one team: in deathmatch with `dmflags` 64 (by
// the skin of `skin`'s model/skin) or 128 (by the model), or in co-op (everyone).
bool fight_teammates(const Q2Server& s, int a, int b);
// Player `slot`'s field of view from his `fov` setting: 1..160, 90 when unset or with `dmflags`
// 32768 in deathmatch.
float fight_fov(const Q2Server& s, int slot);
// A dead player about to respawn leaves his body (not when it was thrown apart).
void fight_bury(Q2Server& s, int slot);
// The scores, best first: one line a player ("  12  Ana").
std::vector<std::string> fight_scores(const Q2Server& s);
// A monster's (map entity `from`'s) bullet from `start` to `end`: players, monsters and walls;
// `cause` names it in an obituary.
void fight_monster_bullet(Q2Server& s, int from, sh::Vec3f start, sh::Vec3f end, int damage,
                          int kick, int cause);
// A monster's missile of `kind` from `start` along `dir` at `speed`.
void fight_monster_launch(Q2Server& s, int from, SqShot kind, sh::Vec3f start, sh::Vec3f dir,
                          int damage, float speed, int cause);
// A monster's BFG ball: the player's ball, its final burst `burst` strong within `reach`.
void fight_monster_bfg(Q2Server& s, int from, sh::Vec3f start, sh::Vec3f dir, float speed,
                       int burst, float reach, int cause);
// A monster's rail from `start` toward `end`: through every player to the first wall or monster.
void fight_monster_rail(Q2Server& s, int from, sh::Vec3f start, sh::Vec3f end, int damage, int kick,
                        int cause);
// A push added straight to player `slot`'s velocity (a monster's blow; not by mass).
void fight_kick(Q2Server& s, int slot, sh::Vec3f push);
// Where a laser's line stopped, and the face it met there (zero at its full reach or the sky).
struct LaserEnd {
  sh::Vec3f at, normal;
};
// A laser's line from `start` to `end`: `damage` to every player and monster on it, the line
// passing on through them, and to the mover or wall that stops it (an energy blow, `attacker` a
// slot or -1).
LaserEnd fight_laser(Q2Server& s, sh::Vec3f start, sh::Vec3f end, int damage, int attacker,
                     int skip_entity);
// A point effect (`te_*`) at `at` for everyone.
void fight_effect(Q2Server& s, const char* name, sh::Vec3f at, sh::Vec3f way = {});
// A line effect `name` from `a` to `b` that monster entity `from` makes (a cable it holds out, a
// shot's flash from its gun along its aim): the window draws it, a QuakeWorld client ignores it.
// `flash`: a monster's muzzle-flash value (server/q2_flash.hpp) the line carries, 0 for none; the
// wire sends it as the protocol's monster flash, the window plays its gun's sound by it.
void fight_monster_line(Q2Server& s, const char* name, int from, sh::Vec3f a, sh::Vec3f b,
                        int flash = 0);
// An item (`classname`) thrown up from `at`, as a monster drops it at death.
void fight_drop_at(Q2Server& s, sh::Vec3f at, const std::string& classname);
// A body thrown apart at `at`: entity row `ent`'s sound, the blood and the pieces `t` names.
void fight_gib(Q2Server& s, int ent, sh::Vec3f at, const SqGibThrow& t);
// A line printed to player `slot`, or to everyone with -1.
void fight_print(Q2Server& s, int slot, const std::string& text, int level = 1);

}  // namespace shooters_edge::server
