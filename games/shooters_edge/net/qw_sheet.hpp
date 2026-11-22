// SHOOTER'S EDGE'S QUAKEWORLD SERVER'S SETTINGS SHEET (engine/server/sheet.hpp): the rows a player
// on it may read and change, and the sheet sent to the clients that ask for it.
//
// THE ROWS: the map (a vote: `vote map <name>`), then the rules a vote may change here (the
// vote's lists and the game's check say which: engine/server/vote.hpp), each on its page: the
// match's (`deathmatch`, `teamplay`, `fraglimit`, `timelimit`, overtime, midair) on the first,
// the player aids on PLAYER HELP (the rocket jump helpers and the aim help's leeway, and each
// player's own grant of the easy jump, the aim help and the rocket jump helper, `vote speedjump
// <id> on`), the move's settings on MOVEMENT, the rest on RULES. A rule the server is started with
// (its endpoint's) is never offered; the first page's rules are shown even when the lists keep
// them from a vote, so the match's limits are always on view. The values are the server's now:
// what a vote made, else what the rules run with.
//
// THE WIRE: a client that agreed the `ge` set's `settings-sheet` bit asks with the word `ge_sheet`
// (`ge_sheet stop`: no more); the server answers with svc `ge_sheet` (a part number and a text:
// begin, a row a message, end, a few each frame) and, while he keeps it, the sheet again whenever a vote changes a
// setting or the map puts one back. A stock QuakeWorld client never agrees the bit, so never asks.
//
// ge:summary Shooter's Edge's QuakeWorld server's settings sheet: the rows and the svc that sends them
// ge:keywords server-settings settings-sheet ge_sheet votable player-help qw_sheet_rows
// ge:keywords qw_send_sheet settings-sheet-bit
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_sheet_test.cpp
#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "server/server.hpp"
#include "server/sheet.hpp"
#include "shooters_edge/net/qw_server.hpp"

namespace shooters_edge::net {

// What the rows are read from: the game's rules, each one's value now and whether the server is
// started with it, the vote's lists, the map, and the players (for their aids; null: none).
struct QwSheetFacts {
  std::span<const ge::server::GameRule> rules;
  std::function<std::string(std::string_view name)> value;
  std::function<bool(std::string_view name)> fixed;  // set when the server starts
  const ge::server::VoteSettings* vote = nullptr;
  std::string map;
  const QwServer* players = nullptr;
};

// The page a rule is on: "" the first, else its words ("Player help", "Movement", "Rules").
std::string qw_sheet_group(std::string_view name);
void qw_sheet_rows(const QwSheetFacts& facts, ge::server::SettingsSheet& out);
// The messages of a sheet waiting to go to one client: a sheet is longer than the reliable
// channel holds at once, so `qw_sheet_pump` sends a few each frame while the channel's queue is
// short (a new sheet replaces what of the last one has not gone yet).
struct QwSheetOut {
  std::vector<std::pair<ge::server::SheetPart, std::string>> parts;
  size_t next = 0;
};
// The sheet queued for the client in `slot` (begin, every row, end); nothing for a local seat
// (the listen server's own player, whose window reads the sheet in process) or a client that did
// not agree the bit.
void qw_send_sheet(QwServer& s, int slot, const ge::server::SettingsSheet& sheet, QwSheetOut& out);
// Each frame: the next parts sent reliably while the channel has room.
void qw_sheet_pump(QwServer& s, int slot, QwSheetOut& out);

}  // namespace shooters_edge::net
