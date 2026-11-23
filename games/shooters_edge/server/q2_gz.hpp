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

#include "shooters_edge/server/q2_combat.hpp"

namespace shooters_edge::server {

struct Q2Server;
struct Q2Missile;
struct Q2Entity;

// Ground Zero's own settings (`strong_mines`, `g_disruptor`), registered only
// when it is the pack played.
void gz_cvars(Q2Server& s);
// The class name a map's entity spawns as under Ground Zero (its rows for the base game's nail
// gun and heat beam, and for the other pack's weapons and ammo); the same name otherwise.
std::string_view gz_classname(std::string_view classname);
// Whether item row `item` stays off the map under Ground Zero's rules: the disruptor and its
// rounds unless `g_disruptor`; in deathmatch, the mines (dmflags 131072), the A-M bomb (524288),
// the spheres (1048576).
bool gz_left_out(const Q2Server& s, int item);

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

// TAG (`gamerules 2` in deathmatch, q2_gz_tag.cpp): one token on the map; its holder scores 3 a
// kill (a quad every fifth), whoever kills him scores 5 and takes it at once, and a blow neither
// he gives nor takes is a quarter less. Dead or gone without a killer to pass it to, he throws it:
// nobody may take it for 1 s, and it is moved to the farthest deathmatch start 30 s later (3 s
// when it lies in lava or slime). The holder trails the token's light and shows `tag3` on the
// scoreboard. `randomrespawn 1`: an item taken comes back as another of its kind.
inline constexpr int kGzTagTrailEffect = 0x20000000;

// `gamerules` and `randomrespawn` (gz_cvars registers them).
void gz_tag_cvars(Q2Server& s);
// Whether the tag game is played: Ground Zero, deathmatch and `gamerules` 2.
bool gz_tag_on(const Q2Server& s);
// Whether item row `item` is the tag token.
bool gz_tag_token(int item);
// Whether item row `item` is left off the map: the token, whenever tag is not played.
bool gz_tag_left_out(const Q2Server& s, int item);
// Player `slot` touches item row `item`: true (and taken when he lives) when it is the token.
bool gz_tag_take(Q2Server& s, int slot, int item);
// The map's start (its items on the floor): a token at a start when the map placed none.
void gz_tag_start(Q2Server& s);
// Player `slot` died to `killer` (`by_killer`: the kill is the killer's): true when tag scored it;
// the holder's token passed to his killer or thrown.
bool gz_tag_kill(Q2Server& s, int slot, int killer, bool by_killer);
// Player `slot` leaves: the token he holds thrown.
void gz_tag_left(Q2Server& s, int slot);
// A blow's `damage` under tag (the same outside it).
int gz_tag_damage(const Q2Server& s, int target, int attacker, int damage);
// Whether player `slot` holds the token.
bool gz_tag_holder(const Q2Server& s, int slot);
// A tenth: a thrown token looked at after 1 s and moved when its time comes.
void gz_tag_frame(Q2Server& s);
// An item `e` taken and shown again: under `randomrespawn`, another of its kind. True when it was
// swapped.
bool gz_random_respawn(Q2Server& s, Q2Entity& e);

}  // namespace shooters_edge::server
