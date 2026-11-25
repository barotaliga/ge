// QUAKE II'S SCREENS: what its rules draw over a player's status bar by a layout program
// (hud/q2_layout.hpp) sent to that player alone, which his client keeps and draws while his
// stat 13 has bit 1. Facts: porting/shooters-edge/q2-hud.md § 6.
//
// - **The scoreboard** (deathmatch and co-op): the players in the game, best score first (equal
//   scores in slot order), at most 12, in two columns of six blocks 32 px apart from `yv 32`; each
//   a `client` block (name, score, ping, minutes in the game), the viewer's own behind a `tag1`
//   dog tag and his last killer's behind `tag2`. Kept under 1024 letters (a block that would not
//   fit, and every one after it, is left out). `score` toggles it; while it is up it is written
//   again every 32 frames so the scores and pings stay current; dying in deathmatch toggles it,
//   respawning closes it, and the intermission opens it for everyone with no killer's tag.
// - **The help computer** (single player and co-op, q2_campaign.hpp): `help` shows it, or
//   hides it when it is up and its news has not changed; in deathmatch `help` is `score`.
// - `score` in single player only closes help; `putaway` closes either screen, `inven` too.
// - **The inventory**: `inven` toggles it (closing the screens; `score`, `help` and `putaway`
//   close it) and sends the player's counts by item number as they are then (not again while it
//   is open); it is drawn while he lives (stat 13 bit 2) and a respawn closes it. The selection
//   commands step the selected item (stat 12) through the held ones, wrapping, or to none:
//   `invnext`/`invprev` every usable item, `invnextw`/`invprevw` the weapons and the grenades,
//   `invnextp`/`invprevp` the five power-ups with a use. `invuse` and `invdrop` first keep the
//   selection if he holds one, else step it on, then use or drop it ("No item to use.").
//
// ge:summary Quake II's scoreboard and help computer as layout programs a player's client draws
// ge:keywords q2 scoreboard help-computer layout screen score putaway inven tag1 tag2
// ge:keywords stat-13 q2-hud layout-message inventory invnext invprev invuse invdrop
// ge:keywords invnextw invprevw invnextp invprevp selected-item
// ge:skill shooter ui
// ge:test tests/shooters_edge_q2_screens_test.cpp
#pragma once

#include <cstdint>
#include <string>

namespace shooters_edge::server {

struct Q2Server;

inline constexpr uint8_t kScreenNone = 0, kScreenScores = 1, kScreenHelp = 2;
inline constexpr size_t kLayoutMost = 1023;    // the longest layout a client keeps
inline constexpr int kSqInventorySlots = 256;  // the inventory message's counts

// The scoreboard's program as player `viewer` sees it (`killer`'s block tagged; -1 none).
std::string q2_scoreboard(const Q2Server& s, int viewer, int killer);

// A player's command the screens answer (`score`, `help`, `putaway`, `inven`, the selection
// commands); false for others.
bool screens_command(Q2Server& s, int slot, const std::string& cmd);
// A rules frame: the scoreboards that are up written again every 32 frames.
void screens_frame(Q2Server& s);
// Player `slot` died: in deathmatch his scoreboard toggles.
void screens_died(Q2Server& s, int slot);
// Player `slot` (re)spawned: his screen closes.
void screens_spawned(Q2Server& s, int slot);
// The intermission began: everyone's scoreboard up (deathmatch and co-op).
void screens_intermission(Q2Server& s);

}  // namespace shooters_edge::server
