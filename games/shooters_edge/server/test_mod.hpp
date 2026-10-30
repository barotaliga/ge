// THE TEST MOD: a small deathmatch mod built with the tree's own assembler (plugins/progs/
// assembler.hpp), the stand-in for a player's own progs until the tree's compiler (M18d) builds
// one from QC source. It calls the standard builtins by their numbers, so the mod server
// (qc_server.hpp) runs it exactly as it runs a mod loaded from a file.
//
// What it does, function by function (as a QC source would read):
//   worldspawn        precaches the player and missile models and two sounds, sets light style
//                     0, asks checkextension for ZQ_PAUSE and FTE_CALLTIMEOFDAY, and calls
//                     calltimeofday when offered (its `timeofday` keeps the hour in `start_hour`)
//   info_player_deathmatch  stays as a spawn point (its origin and angle from the entity text)
//   SetNewParms       parm1 100 (health), parm2 0; SetChangeParms keeps health and frags
//   ClientConnect     bprint "<name> entered the game"
//   PutClientInServer the next spawn point round the map (find from the last one, wrapping to the
//                     world), a player: health 100, takedamage 2, solid slidebox, movetype walk,
//                     the player model and box, view 22 up, at the spot facing its angles
//                     (fixangle), maxspeed 320 and gravity 1 (the extension fields)
//   PlayerPostThink   attack held, the match on and the weapon ready (0.8 s between shots): fires
//                     a rocket: makevectors(v_angle), a sound, `newmis` a flying missile at 1000
//                     u/s along v_forward from 8 ahead and 16 up, its angles from vectoangles,
//                     removed after 5 s
//   missile_touch     not its owner: an explosion message (WriteByte TE_EXPLOSION, three
//                     WriteCoord, multicast PHS), a sound, then every entity findradius finds
//                     within 120 that takes damage gets 100 - half its distance (when above 0),
//                     then the missile is removed
//   T_Damage          health down; at or below 0 the attacker's frags up (or the victim's down for
//                     his own rocket), logfrag, an obituary bprint, and the victim respawns at once
//   ClientKill        T_Damage on himself, 1000
//   StartFrame        counts `frames`; once `timelimit` minutes pass: `match_over`, bprint
//                     "Timelimit hit.", changelevel to the same map
//   UserCmd           "frags": sprint the player's frags, handled; anything else not handled
//   ChatMessage       "gg": bprint "<name> says good game", handled; else not
//   ClientDisconnect  bprint "<name> left the game with <frags> frags"
//
// `kArenaEntities` is the test arena's entity text for it: the world (with a `wad` key, which is
// no field, and a `_comment` ignored), four spawn points, a health box that is not in deathmatch
// (spawnflags 2048: inhibited) and a light with no spawn function (skipped with a message).
//
// THE SINGLE-PLAYER STUB (`build_single_player_stub`): a program of the single-player form (crc
// 5927) whose only functions are the spawn functions `worldspawn` and `target`, each returning at
// once, for the tests and the `se-campaign` lab to stand that form on the mod server with;
// `kStubEntities` is a world for it.
//
// ge:summary The assembled deathmatch test mod, the test arena's entities, the single-player stub
// ge:keywords test-mod deathmatch assembled-mod rocket missile frags timelimit spawn-points
// ge:keywords build_test_mod karenaentities entity-text worldspawn info_player_deathmatch
// ge:keywords build_single_player_stub single-player-stub kstubentities
// ge:skill progs shooter
// ge:test tests/shooters_edge_mod_match_test.cpp tests/shooters_edge_qc_single_player_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "progs/file.hpp"

namespace shooters_edge::server {

// Loads the test mod into `out`. False with `error` only if the assembler broke.
bool build_test_mod(ge::progs::Progs& out, std::string& error);

// The test mod as a progs file's bytes (what `--progs` reads), with its line table in `lines`.
std::vector<uint8_t> test_mod_bytes(std::vector<int32_t>& lines);

extern const char* const kArenaEntities;

// The single-player stub into `out`. False with `error` only if the assembler broke.
bool build_single_player_stub(ge::progs::Progs& out, std::string& error);
extern const char* const kStubEntities;

// The test mod's fire rate and rocket, for the tests and the bots.
constexpr float kRefire = 0.8f;       // s between shots
constexpr float kRocketSpeed = 1000;  // units/s
constexpr float kBlastRadius = 120;   // units

}  // namespace shooters_edge::server
