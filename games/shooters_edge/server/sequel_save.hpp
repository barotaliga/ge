// THE SEQUEL'S LEVELS KEPT AND ITS GAMES SAVED (the clean-room note
// porting/shooters-edge/sequel-campaign.md § Units and cross-level state, § Saving and loading;
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
//   settled and before the players come in, the rules make an autosave (`SequelServer::autosave`)
//   for the host to keep as slot `save0`: the game without the level, so a load puts the players
//   at the spawn point with what they carried in.
// - **A save by hand** (`sequel_save`): refused with no game, in deathmatch, at a level's end,
//   with C++ mods on, and in single player while the player is dead; allowed in co-op. It holds
//   the game, the unit's kept levels, the level now and every player's body.
// - **A load** (`sequel_load`): the saved settings set, the map started again and settled two
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
// THE CONTRACT the tests and the `se-sequel-save` lab hold: a game saved at a frame, loaded and
// run on, has at every later frame the `sequel_digest` of the game that was never saved.
//
// ge:summary The sequel's levels kept within a unit and its games saved and loaded on engine/save
// ge:keywords sequel save load savegame loadgame autosave save0 revisit kept-level unit
// ge:keywords sv_noreload sequel_save sequel_load sequel_digest menu_loadgame crosslevel
// ge:skill shooter save
// ge:test tests/shooters_edge_sequel_save_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace shooters_edge::server {

struct SequelServer;
struct RulesRunner;
struct MapSpec;

// The setting the kept levels read: `sv_noreload`.
void sequel_save_cvars(SequelServer& s);

// A map is starting (before the old one goes): the level being left kept for its unit, or the
// unit's levels forgotten, by the change asked for.
void sequel_leave(SequelServer& s);

// The fresh map's state replaced by its kept one or a load's (the settle, the read, the cross-level
// targets re-armed, the 10 s run), then the autosave made. False with `error` when kept bytes do
// not read.
bool sequel_enter(SequelServer& s, std::string& error);

// The frame's end for the dead of single player: a button a second after dying asks for the load
// menu.
void sequel_save_frame(SequelServer& s);

// Why the game cannot be saved now, or "" when it can.
std::string sequel_save_problem(const SequelServer& s);

// The game's save into `out` (`saved_unix` the caller's clock, for the menu's list). False with
// `error` when it cannot be saved now.
bool sequel_save(const SequelServer& s, std::vector<uint8_t>& out, std::string& error,
                 int64_t saved_unix = 0);

// The map a save `bytes` was made on ("" with `error` when it is no save of this game).
std::string sequel_save_map(std::span<const uint8_t> bytes, std::string_view name,
                            std::string& error);

// The save `bytes` (named `name` in a refusal) asked of the sequel's server `s`: checked, and
// kept until the next map starts (`RulesRunner::start` on the save's map, the one returned), which
// loads it. "" with `error` when the file is damaged, of another game or newer than this build.
std::string sequel_load_ask(SequelServer& s, std::span<const uint8_t> bytes, std::string_view name,
                            std::string& error);

// The same, the save's `map` started at once: a new runner takes every saved player; a running
// game is let go and those connected to it play on, each in his saved body or coming in afresh.
// False with `error` when the file is refused (the game before it left playing) or its level or
// players do not read.
bool sequel_load(const RulesRunner& r, const MapSpec& map, std::span<const uint8_t> bytes,
                 std::string_view name, std::string& error);

// A map starting with a load asked (sequel_server.cpp's start): before it, the save's settings
// and game; after it, its players. False with `error` when the file does not read.
bool sequel_load_begin(SequelServer& s, std::string& error);
bool sequel_load_end(SequelServer& s, std::string& error);

// A digest of the running game: the loop's, the level, the players and the game, bit for bit.
uint64_t sequel_digest(const SequelServer& s);

}  // namespace shooters_edge::server
