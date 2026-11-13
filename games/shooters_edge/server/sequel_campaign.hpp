// THE SEQUEL'S CAMPAIGN: what holds a single-player (or co-op) game together across its levels,
// run by the sequel runner (sequel_server.hpp) from the clean-room note
// porting/shooters-edge/sequel-campaign.md.
//
// - **The help computer**: two game-wide messages a `target_help` replaces (sequel_map.cpp), kept
//   for the whole game. A change marks every player's help unread: up to three beeps
//   (`misc/pc_up.wav`) on frames whose number is a multiple of 64, and the blinking icon until
//   the player opens it with `help` (in deathmatch `help` is `score`). The screen: the skill, the
//   level's name, both messages and the kills, goals and secrets found of their totals.
// - **What a player carries**: on a level change (not in deathmatch) his whole body (health cut
//   to its maximum, armour, weapons, ammo and their limits, keys, unused powerups); a dead player,
//   or a new game (`map`), starts afresh. In co-op a unit's end (`*`) takes every key, and a dead
//   player comes back as he entered the level.
// - **Where he comes in**: the spawn point the exit named (`$name`): the `info_player_start` of
//   that name (letter case aside); with none named, the first unnamed one, else the first of any.
//   In co-op the second to fourth players take the matching `info_player_coop`s in order.
// - **The cross-level bits**: eight flags `target_crosslevel_trigger`s set and
//   `target_crosslevel_target`s read; a unit's end clears them, a new game too.
//
// ge:summary The sequel's campaign: the help computer, the body carried across levels, spawn points
// ge:keywords sequel campaign help-computer target-help carried-body spawn-point crosslevel coop
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_campaign_test.cpp
#pragma once

#include <string>

namespace shooters_edge::server {

struct SequelServer;
struct SequelEntity;

// A map starts: the game goes on (a level change), restarts the level, or begins anew, by the
// change that started it; the players' bodies are set aside to be carried.
void campaign_start(SequelServer& s);
// The spot player `slot` comes in at outside deathmatch; null when the map has none.
const SequelEntity* campaign_spot(const SequelServer& s, int slot);
// Player `slot` was just put in the world (`respawn` after a death): the body he carries.
void campaign_body(SequelServer& s, int slot, bool respawn);
// The frame's help news: unread marks and their beeps.
void campaign_frame(SequelServer& s);
// A player's command the campaign answers (`help`, `putaway`, `inven`); false for others.
bool campaign_command(SequelServer& s, int slot, const std::string& cmd);
// The help computer's screen as player `slot` sees it, line by line.
std::string campaign_help_screen(const SequelServer& s);

}  // namespace shooters_edge::server
