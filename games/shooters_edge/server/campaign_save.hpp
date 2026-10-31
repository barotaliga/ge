// THE CAMPAIGN SAVED AND LOADED (porting/shooters-edge/campaign.md § Saves; docs/saves.md is the
// file): a single-player game written to an engine/save file and read back so that it plays on
// as if never stopped. A save is the state, never the inputs: the map is started again from its
// name (its spawn functions run, the brush models, the precache, the hooks rebuilt), then every
// part of the running game the frame can reach is read over it.
//
// THE CHUNKS, each version 1:
// - `CAMP` what the campaign carries: the program's identity (its crc, its sizes and a hash of its
//   code: a save is refused by another build of the program), the map, `serverflags` and what the
//   map was entered with, every player slot's parms, the changed settings (the cvars, so the map
//   restarts under the same rules) and the history.
// - `QSRV` the mod server's: the light styles, the clients (their user info and spawn parms), the
//   static entities, the precache lists, the per-entity alpha and colour, the pause, the server
//   and local info, the command buffer, the checkclient candidate.
// - `LOOP` the loop's: the server's time and frames, the globals, every entity slot in use in
//   full (its links in the area tree included), the area tree's lists (so the touch order is the
//   same), every client slot's command state, the touch stamps.
// - `QCVM` the program's: its globals word for word, every entity's fields, every string the
//   engine, the temporaries and the zone hold (so a string value keeps meaning what it meant),
//   the tokenizer's words, which entities are live in the mirror, and the generator's state.
//
// A save is refused (`campaign_save_problem`) when no map is running, the program is not the
// single-player form, the game is not single player (coop, deathmatch, a second player), or the
// player is dead. The intermission is the client's to refuse (it is read off the program's
// messages: client/listen.hpp's `listen_save`).
//
// THE CONTRACT the tests and the `se-campaign` lab hold: a game saved at a frame, loaded and run
// on, has at every later frame the `campaign_digest` of the game that was never saved.
//
// ge:summary The campaign saved and loaded on engine/save: the whole running game, bit for bit
// ge:keywords save load savegame campaign_save campaign_load campaign_digest chunk single-player
// ge:keywords continue restore quicksave state-digest
// ge:skill shooter save
// ge:test tests/shooters_edge_campaign_save_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/server/campaign.hpp"

namespace shooters_edge::server {

inline constexpr const char* kSaveGame = "shooters_edge";  // the file's game (Meta::game)
inline constexpr uint32_t kSaveVersion = 1;                // the game's save version

// Why the campaign cannot be saved now, or "" when it can.
std::string campaign_save_problem(const Campaign& c);

// The campaign's save into `out` (`saved_unix` the caller's clock, for the menu's list). False
// with `error` when it cannot be saved now.
bool campaign_save(const Campaign& c, std::vector<uint8_t>& out, std::string& error,
                   int64_t saved_unix = 0);

// The save `bytes` (named `name` in a refusal) loaded into `c` for `progs` with `opt` (the map
// loader's): the map started again and the save's state read over it. False with `error` when
// the file is damaged, of another program or does not read; `c` then has no server.
bool campaign_load(Campaign& c, const pg::Progs& progs, const CampaignOptions& opt,
                   std::span<const uint8_t> bytes, std::string_view name, std::string& error);

// A digest of the running game: the loop's (`server_hash`) and the program's globals and every
// entity's fields, bit for bit.
uint64_t campaign_digest(const QcServer& s);

}  // namespace shooters_edge::server
