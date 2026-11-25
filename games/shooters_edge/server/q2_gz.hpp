// GROUND ZERO'S RULES (the Quake II mission pack in the `rogue` folder), beside Quake II's own:
// its weapons, its thrown mines and bomb, and the items only it has. Everything here answers only
// to the pack's rows (q2_pack.hpp), so the base game's maps, goldens and digests never meet it.
//
// - **The chainfist**: no ammo; from frame 6 it picks one of three swings (A frames 6..13 hitting
//   on 8 and 9, B 14..23 on 16..18, C 24..32 on 30 and 31; never the one just played while held),
//   each hit reaching 64 units from right 8, eye height − 4: 30 in deathmatch, 15 outside it, no
//   push, past armour; on a hit the wielder is pulled 75 forward and 75 up. More blood (TE 42) on a
//   body, a puff (TE 0) on a wall.
// - **The ETF rifle**: a flechette every 0.1 s held (frames 6 and 7, right 8 then 6), 750 units/s,
//   10 damage and a push of 3, past armour; its mark (TE 55) on a wall; muzzle flash 30.
// - **The prox launcher**: the grenade launcher's frames; a mine thrown at 600 (and 200 ± 10 up)
//   sticks to the world, opens for 1 s, and goes off at once when it arms in sight of a body
//   within 202 units (or, in deathmatch, of a spawn or a teleporter's exit); armed, a body within
//   ±96 sets it off 0.5 s later. 90 within 192 (× the multiplier); 45, 30, 15 or 10 s to live by
//   the multiplier (45 with `strong_mines`). Muzzle flash 8.
// - **The plasma beam**: 2 cells a frame on frames 9..12, looping while held: a line 8192 long,
//   15 a frame (energy), a push of 75 in deathmatch, 30 outside it; halved past a water surface
//   (its sparks, TE 43, and bubbles, TE 41); steam (TE 44) on a wall; the beam (TE 38) to everyone.
//   Muzzle flash 33.
// - **The disruptor**: a bolt at 1000 that turns onto what the aim's line met each 0.1 s; on a
//   body it pushes (3 × damage, and 140 up) and hurts it a fifth of 30 (deathmatch) or 45 every
//   0.1 s for 0.5 s; its burst TE 47. It takes its round even with infinite ammo. Muzzle flash 35.
// - **The tesla**: thrown as a grenade is held (hold on frame 1, thrown on 2, 400 to 800 units/s),
//   it rests, opens 3 s later over 15 frames and then for 30 s zaps every monster (and, in
//   deathmatch, every player) within ±128 across and 128 up that it sees: 3 (× the multiplier) and
//   a push of 8, the bolt TE 33. In water, slime or lava it blows up: 150 within 200.
// - **The A-M bomb**: used from the inventory, thrown at 100 (200 up); 10 s on it bursts: 10 000
//   within 512 (more with the multiplier), falling to nothing at twice that, TE 53 and 51 to all;
//   then 3 s of quake throwing everyone on the ground. Beeps all the while.
// - **Double damage** (q2_powers.hpp, q2_fight_kit.hpp's multiplier) and the **IR goggles**.
//
// ge:summary Ground Zero's rules beside Quake II's: chainfist, ETF, prox, plasma beam, disruptor
// ge:keywords q2 ground-zero rogue mission-pack chainfist etf-rifle flechette prox-mine tesla
// ge:keywords plasma-beam heat-beam disruptor tracker am-bomb nuke double-damage ir-goggles
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_test.cpp
#pragma once

#include <string_view>

#include "shooter/vec.hpp"
#include "shooters_edge/server/q2_combat.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;

struct Q2Server;
struct Q2Missile;

// Ground Zero's own settings (`strong_mines`, `g_disruptor`, `huntercam`), registered only
// when it is the pack played.
void gz_cvars(Q2Server& s);
// The class name a map's entity spawns as under Ground Zero (its rows for the base game's nail
// gun and heat beam, and for the other pack's weapons and ammo); the same name otherwise.
std::string_view gz_classname(std::string_view classname);
// Whether item row `item` stays off the map under Ground Zero's rules: the disruptor and its
// rounds unless `g_disruptor` (every mode); outside deathmatch the A-M bomb, the Doppleganger and
// the hunter and vengeance spheres; in deathmatch, the mines (dmflags 131072), the A-M bomb
// (524288), the spheres (1048576).
bool gz_left_out(const Q2Server& s, int item);
// Ground Zero's co-op spawn rule for spawnflags `flags` (outside deathmatch): 1 left out (4096,
// never in co-op), 0 kept (all three skill bits: co-op only, at every skill), -1 the base game's
// rule decides (single player, another pack, `coop_baseq2` 1, or any other flags).
int gz_coop_left_out(const Q2Server& s, int flags);

// A shot of Ground Zero's weapon in `slot`'s hand (q2_fight.cpp's fire hands it over).
void gz_fire(Q2Server& s, int slot, int shots);
// A tenth of shot `m` when it is one of Ground Zero's (its flight, a mine's stages, the bomb's
// count down); false for any other kind, which flies by the base game's rules.
bool gz_missile(Q2Server& s, Q2Missile& m);
// Whether shot `m` is drawn (a disruptor hurting what it struck, the bomb's quake are not).
bool gz_drawn(const Q2Missile& m);
// Player `slot` uses an A-M bomb from the inventory: thrown.
void gz_throw_nuke(Q2Server& s, int slot);

// The hand's fire run for Ground Zero's weapons (q2_combat.cpp's step hands it over): the
// chainfist's swings, the ETF rifle's and the plasma beam's loops, the tesla's hold and throw.
SqStep gz_hand_fire(SqHand& h, SqBody& body, bool attack, double now, bool infinite);
// The weapon a body that runs dry turns to in Ground Zero: railgun, plasma beam (2 cells or more),
// ETF rifle, chaingun, machinegun, super shotgun (2 shells or more), shotgun, blaster.
int gz_best_weapon(const SqBody& body);

// --- The spheres (q2_gz_spheres.cpp)
// -------------------------------------------------------------- Each launched from the top of its
// owner's box and living 30 s, one out a player at a time, following its owner 4 above his box and
// leaping to that spot each whole second it loses sight of him; drawn full bright and seen through
// IR goggles; harmless bursts (TE 5) at its end, at the intermission, at `kill` and when its owner
// leaves.
// - **Defender**: halves its owner's blows (at least 1) before anything else saves them, and every
//   0.4 s at most shoots a green bolt (1000 units/s, 10 and a 30 splash within 128, never on its
//   owner) at whoever last hurt him while it sees him; it bursts when its owner dies.
// - **Hunter**: idle, it turns to its owner's yaw. Its owner killed by another, it hunts the
//   killer (15 s at least left): at 500 straight at his eye while it sees him, else to where it
//   last saw him, lurking there; a body it touches takes 10 000 past armour, a thing that takes no
//   blows a 512 blast within 256; with `huntercam` and the killer 192 or more away, its owner's
//   corpse is thrown apart and his view rides it (field of view 140).
// - **Vengeance**: spins; once a blow leaves its owner under 25, it flies at 500 straight at the
//   attacker (15 s at least left) and strikes as the hunter does, the world too.
// A hunter or vengeance sphere with no prey yet bursts when its owner dies; one hunting goes on.

// Whether `k` is a sphere's kind.
constexpr bool gz_sphere_kind(SqShot k) {
  return k == SqShot::defender_sphere || k == SqShot::hunter_sphere ||
         k == SqShot::vengeance_sphere;
}
// Player `slot` uses sphere item `item`: launched, or kept with "Only one sphere at a time!".
// False when `item` is no sphere.
bool gz_use_sphere(Q2Server& s, int slot, int item);
// Whether player `slot` may not pick up item `item` (a sphere, while he has one out).
bool gz_sphere_refused(const Q2Server& s, int slot, int item);
// A blow of `damage` on player `target` from `by` (a slot, a monster as -2 - its entity, -1 none):
// the defender's half (at least 1) and its new target. Returns the damage left.
int gz_sphere_hurt(Q2Server& s, int target, int by, int damage);
// A blow from `by` that player `target` lived through: the vengeance sphere's call under 25.
void gz_sphere_hit(Q2Server& s, int target, int by);
// Player `slot` killed by `killer` (a slot, -1 none): his sphere bursts, or hunts the killer.
void gz_sphere_died(Q2Server& s, int slot, int killer);
// Player `slot`'s sphere gone at once (the `kill` command), harmlessly.
void gz_sphere_remove(Q2Server& s, int slot);
// A doppleganger of `owner` killed by `target` (a slot) releases a sphere at `from`: a hunter or a
// vengeance one, hunting at once for 15 s, its kills the doppleganger's, never hurting its owner.
void gz_release_sphere(Q2Server& s, int owner, int target, sh::Vec3f from, bool hunter);
// The Doppleganger (q2_gz_decoy.cpp), each a no-op outside the pack: a decoy row for every slot
// at a deathmatch map's start; player `slot` using item `item` (true when it was the Doppleganger,
// set down or kept); a blow on entity `i` (true when it is a decoy); its tenth (the fall, the
// turns, its end); its rows.
void gz_decoys_reserve(Q2Server& s);
bool gz_use_decoy(Q2Server& s, int slot, int item);
bool gz_decoy_damage(Q2Server& s, int i, int attacker, int damage);
void gz_decoys_frame(Q2Server& s);
void gz_decoy_rows(Q2Server& s);
// Whether player `slot`'s view rides a hunter sphere (or stopped where one ended).
bool gz_sphere_view(const Q2Server& s, int slot);
// A tenth of sphere `m`, or of a defender's bolt (`m` a green bolt a player owns); false for
// anything else.
bool gz_sphere_missile(Q2Server& s, Q2Missile& m);

}  // namespace shooters_edge::server
