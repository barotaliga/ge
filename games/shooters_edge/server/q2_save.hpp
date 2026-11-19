// QUAKE II'S LEVELS KEPT AND ITS GAMES SAVED (the clean-room note
// porting/shooters-edge/q2-campaign.md § Units and cross-level state, § Saving and loading;
// docs/saves.md is the file): a level left within a unit kept and read back when it is entered
// again, and a single-player or co-op game written to an engine/save file and read back so that
// it plays on as if never stopped.
//
// - **A level kept**: left by an exit to a map of the same unit (no `*`), not in deathmatch, its
//   whole state (every entity, monster, shot in flight, drop and body, the delayed uses, the area
//   portals, the light styles, its counters and its time) is kept without the players. An exit
//   to a new unit (`*`) and a new game (`map`) forget the unit's levels.
// - **A level entered again** (not in deathmatch, unless `sv_noreload` is 1): the map starts fresh
//   and runs two frames with nobody in it (what they do to the game, a help message or a
//   cross-level bit, stays), then the kept state is read over it, so the dead stay dead, opened
//   doors open and taken items gone, and its time goes on. Every cross-level target not yet fired
//   checks again its `delay` from then. The level then runs 10 s alone before the players come in
//   at the spawn point the exit named, with what they carry.
// - **The autosave**: as every level is entered outside deathmatch (an exit or `map`), once it has
//   settled and before the players come in, the rules make an autosave (`Q2Server::autosave`)
//   for the host to keep as slot `save0`: the game without the level, so a load puts the players
//   at the spawn point with what they carried in.
// - **A save by hand** (`q2_save`): refused with no game, in deathmatch, at a level's end,
//   with C++ mods on, and in single player while the player is dead; allowed in co-op. It holds
//   the game, the unit's kept levels, the level now and every player's body.
// - **A load** (`q2_load`): the saved settings set, the map started again and settled two
//   frames, the saved level read over it (no 10 s run), and each player back in his body; from an
//   autosave, each comes in at the spawn point with what he carried.
// - **Death in single player**: a button pressed a second or more after dying asks the player's
//   client to open the load menu (`menu_loadgame`); nothing is loaded by itself.
//
// THE CHUNKS, each version 1: `SQGM` the game (its map, the changed settings, the help messages,
// the spawn point, the cross-level bits, the kept levels, the generator, each slot's record as it
// entered the level and what it carries), `SQLV` the level now, `SQPL` the players (their client
// records and bodies). An autosave has no `SQLV` and no `SQPL`.
//
// THE CONTRACT the tests and the `se-q2-save` lab hold: a game saved at a frame, loaded and
// run on, has at every later frame the `q2_digest` of the game that was never saved.
//
// ge:summary Quake II's levels kept within a unit and its games saved and loaded on engine/save
// ge:keywords q2 save load savegame loadgame autosave save0 revisit kept-level unit
// ge:keywords sv_noreload q2_save q2_load q2_digest menu_loadgame crosslevel
// ge:skill shooter save
// ge:test tests/shooters_edge_q2_save_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "shooter/map_spec.hpp"

namespace shooters_edge::server {

struct Q2Server;
struct RulesRunner;
using ge::shooter::MapSpec;

// The setting the kept levels read: `sv_noreload`.
void q2_save_cvars(Q2Server& s);

// A map is starting (before the old one goes): the level being left kept for its unit, or the
// unit's levels forgotten, by the change asked for.
void q2_leave(Q2Server& s);

// The fresh map's state replaced by its kept one or a load's (the settle, the read, the cross-level
// targets re-armed, the 10 s run), then the autosave made. False with `error` when kept bytes do
// not read.
bool q2_enter(Q2Server& s, std::string& error);

// The frame's end for the dead of single player: a button a second after dying asks for the load
// menu.
void q2_save_frame(Q2Server& s);

// Why the game cannot be saved now, or "" when it can.
std::string q2_save_problem(const Q2Server& s);

// The game's save into `out` (`saved_unix` the caller's clock, for the menu's list). False with
// `error` when it cannot be saved now.
bool q2_save(const Q2Server& s, std::vector<uint8_t>& out, std::string& error,
             int64_t saved_unix = 0);

// The map a save `bytes` was made on ("" with `error` when it is no save of this game).
std::string q2_save_map(std::span<const uint8_t> bytes, std::string_view name, std::string& error);

// The save `bytes` (named `name` in a refusal) asked of Quake II's server `s`: checked, and
// kept until the next map starts (`RulesRunner::start` on the save's map, the one returned), which
// loads it. "" with `error` when the file is damaged, of another game or newer than this build.
std::string q2_load_ask(Q2Server& s, std::span<const uint8_t> bytes, std::string_view name,
                        std::string& error);

// The same, the save's `map` started at once: a new runner takes every saved player; a running
// game is let go and those connected to it play on, each in his saved body or coming in afresh.
// False with `error` when the file is refused (the game before it left playing) or its level or
// players do not read.
bool q2_load(const RulesRunner& r, const MapSpec& map, std::span<const uint8_t> bytes,
             std::string_view name, std::string& error);

// A map starting with a load asked (q2_server.cpp's start): before it, the save's settings
// and game; after it, its players. False with `error` when the file does not read.
bool q2_load_begin(Q2Server& s, std::string& error);
bool q2_load_end(Q2Server& s, std::string& error);

// A digest of the running game: the loop's, the level, the players and the game, bit for bit.
uint64_t q2_digest(const Q2Server& s);

}  // namespace shooters_edge::server
