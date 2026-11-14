// THE SEQUEL'S SCREENS: what its rules draw over a player's status bar by a layout program
// (hud/sequel_layout.hpp) sent to that player alone, which his client keeps and draws while his
// stat 13 has bit 1. Facts: porting/shooters-edge/sequel-hud.md § 6.
//
// - **The scoreboard** (deathmatch and co-op): the players in the game, best score first (equal
//   scores in slot order), at most 12, in two columns of six blocks 32 px apart from `yv 32`; each
//   a `client` block (name, score, ping, minutes in the game), the viewer's own behind a `tag1`
//   dog tag and his last killer's behind `tag2`. Kept under 1024 letters (a block that would not
//   fit, and every one after it, is left out). `score` toggles it; while it is up it is written
//   again every 32 frames so the scores and pings stay current; dying in deathmatch toggles it,
//   respawning closes it, and the intermission opens it for everyone with no killer's tag.
// - **The help computer** (single player and co-op, sequel_campaign.hpp): `help` shows it, or
//   hides it when it is up and its news has not changed; in deathmatch `help` is `score`.
// - `score` in single player only closes help; `putaway` closes either screen, `inven` too.
//
// ge:summary The sequel's scoreboard and help computer as layout programs a player's client draws
// ge:keywords sequel scoreboard help-computer layout screen score putaway inven tag1 tag2
// ge:keywords stat-13 sequel-hud layout-message
// ge:skill shooter ui
// ge:test tests/shooters_edge_sequel_screens_test.cpp
#pragma once

#include <cstdint>
#include <string>

namespace shooters_edge::server {

struct SequelServer;

inline constexpr uint8_t kScreenNone = 0, kScreenScores = 1, kScreenHelp = 2;
inline constexpr size_t kLayoutMost = 1023;  // the longest layout a client keeps

// The scoreboard's program as player `viewer` sees it (`killer`'s block tagged; -1 none).
std::string sequel_scoreboard(const SequelServer& s, int viewer, int killer);

// A player's command the screens answer (`score`, `help`, `putaway`, `inven`); false for others.
bool screens_command(SequelServer& s, int slot, const std::string& cmd);
// A rules frame: the scoreboards that are up written again every 32 frames.
void screens_frame(SequelServer& s);
// Player `slot` died: in deathmatch his scoreboard toggles.
void screens_died(SequelServer& s, int slot);
// Player `slot` (re)spawned: his screen closes.
void screens_spawned(SequelServer& s, int slot);
// The intermission began: everyone's scoreboard up (deathmatch and co-op).
void screens_intermission(SequelServer& s);

}  // namespace shooters_edge::server
