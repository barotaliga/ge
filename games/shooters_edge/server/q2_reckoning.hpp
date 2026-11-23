// THE RECKONING'S RULES (the Quake II mission pack in the `xatrix` folder), beside Quake II's own:
// what only its maps place (map things here; its weapons, items and monsters in their tables).
// The numbers are the clean-room notes' (porting/shooters-edge/q2-reckoning-rules.md and
// q2-reckoning-monsters.md); everything here answers only to the pack's class names, so the base
// game's maps, goldens and digests never meet it.
//
// - **The repair object** (`func_object_repair`, renamed `object_repair` for the repair bot to
//   find): 100 health, welding sparks (10, palette 224..231) every `delay` (1 s) while its health is
//   0 or more; welded below 0, its targets fire once (itself the activator), then it regrows a point
//   every `delay` to 101, quietly until past 100.
// - **The rotating light** (`rotating_light`): the spinning lights' effect while on; spawn flags 1
//   starts off, 2 the alarm (`misc/alarm.wav`, static attenuation, every 1 s from 0.1 s after a use
//   turns it on); a use toggles it; 10 health (a map's own kept): broken, 30 sparks and gone.
// - **The hum** (`misc_amb4`): `world/amb4.wav` heard everywhere, 1 s in and every 2.7 s.
// - **The nuke** (`misc_nuke`): its one use strikes every player for 100 000 past invulnerability
//   (god mode still stops it) and takes every monster away outright, uncounted.
// - **The viper's missile** (`misc_viper_missile`): a use fires a monster's rocket (500 units/s,
//   `dmg` 250, its splash `dmg` within `dmg` + 20) at its target's place, the rocket's flash 57,
//   and it goes a frame later.
// - **The shots** (q2_reckoning_shots.cpp): the ion ripper's bolt sparks (welding sparks, none
//   drawn, palette 228..231) where its 3 s end; the beta maiden's heat rocket re-aims at 500 units/s
//   every 0.1 s at the nearest living player in front of it it sees within 1024.
// - **The trap**: thrown as a hand grenade (400 to 800 units/s), it thinks from 1 s on, every 0.1 s
//   on the ground: opens over frames 0..4 (then glows), pulls the nearest living player or monster
//   it sees within 256 (the thrower too: a player +250 units/s toward it a think, a monster at 256);
//   one within 32 under 400 mass dies past invulnerability (100 000, its words "caught in trap"),
//   one heavier bursts it (explosion 1, no harm). Fed, it counts down from 62 by 2 a think, shows
//   frames 6..8 from 18 on and leaves a food cube (Health: the mass / 4 in deathmatch, / 10
//   otherwise, past the maximum), going a second later. Unfed, it bursts quietly at 30 s.
//
// ge:summary The Reckoning's rules beside Quake II's: map things, weapons, items, monsters
// ge:keywords q2 reckoning mission-pack xatrix object-repair rotating-light misc-nuke amb4
// ge:keywords viper-missile pack-rules
// ge:skill shooter
// ge:test tests/shooters_edge_q2_reckoning_test.cpp
#pragma once

#include <string_view>

#include "shooter/vec.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;
struct Q2Server;

// The nuke's blow: no words of its own ("<victim> died.").
inline constexpr int kSqByNuke = 112;
// The spinning lights' effect bit (a rotating light while on).
inline constexpr int kSqSpinningLightsEffect = 0x00800000;
// The hum's period, s.
inline constexpr double kAmbientEvery = 2.7;

// Whether `classname` is one of the pack's map things here.
bool reckoning_thing(std::string_view classname);
// Map entity `i` set up when it is one of them (false otherwise).
bool reckoning_spawn(Q2Server& s, int i);
// A use of map entity `i` when it is one of them (false otherwise).
bool reckoning_use(Q2Server& s, int i, int activator);
// A blow of `damage` to map entity `i` when it is one of them (false otherwise).
bool reckoning_damage(Q2Server& s, int i, int damage);
// One 10 Hz tenth of them.
void reckoning_frame(Q2Server& s);
// Their world rows.
void reckoning_rows(Q2Server& s);

struct Q2Missile;
// A thrown trap's flight from its owner's `forward`, `right` and `up` at `speed`.
void reckoning_trap_launch(Q2Server& s, Q2Missile& m, sh::Vec3f forward, sh::Vec3f right,
                           sh::Vec3f up, float speed);
// A shot at the end of its life: the ion ripper's sparks, the trap's burst (the rest leave none).
void reckoning_missile_end(Q2Server& s, Q2Missile& m);
// A shot's own think before it moves: the heat rocket's aim, the trap's pull; true when it does
// not move this frame (a trap on the ground).
bool reckoning_missile_think(Q2Server& s, Q2Missile& m);

}  // namespace shooters_edge::server
