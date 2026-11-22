// THE WINDOW'S VIEW OF A SERVER'S SETTINGS SHEET (engine/server/sheet.hpp), whichever server it
// is on, for the menu's SERVER SETTINGS pages (menu/server_page.hpp):
//
// - its own LISTEN server: the sheet read in process (`RemoteSheet::local`, the game's
//   ServerGame::sheet) each time the page asks;
// - OUR server joined (it agreed the `ge` set's `settings-sheet` bit): asked with `ge_sheet`, told
//   again by the server whenever a vote changes a setting, and let go with `ge_sheet stop`;
// - a stock QUAKEWORLD server: read from the server info (classic/server_sheet.hpp), read again as
//   it changes; on its match mod, the listings `status1` and `status2` asked for the rest;
// - a QUAKE II server: `info` asked (Quake II sends no server info to a joined client), and on its
//   match mod `settings` once `info` names it. Nothing else is ever sent a stock Quake II server:
//   its game says every word it does not know as the player's chat.
//
// A listing's lines are taken out of the prints for `kListenSeconds` after it was asked, so they
// fill the page and never reach the console; any other line passes. The page's changes go out as
// console lines (`sheet_change_lines`) and the listings are asked again a moment later
// (`remote_sheet_changed`), so the page shows what the server made of them.
//
// The console word the menu drives it by: `serversheet open|refresh|changed|close`.
//
// ge:summary The window's view of a server's settings sheet: ours, a stock server's, a listen's
// ge:keywords server-settings settings-sheet serversheet ge_sheet listing-lines status1 info
// ge:keywords stock-server listen-server
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_sheet_test.cpp
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "classic/server_sheet.hpp"
#include "server/sheet.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::net {

struct Remote;

constexpr double kListenSeconds = 3.0;  // a listing's lines are taken for this long after asking

struct RemoteSheet {
  ge::server::SettingsSheet sheet;  // what the page shows
  ge::classic::SheetSource source = ge::classic::SheetSource::none;
  // The window's own server's sheet, filled in process; false when no listen server runs.
  std::function<bool(ge::server::SettingsSheet&)> local;
  bool open = false;         // the page is up
  double listen_until = -1;  // listing lines are taken until then (the window's clock, s)
  double ask_at = -1;        // the listings are asked again then (after a change; -1 none)
  uint32_t ours_seen = 0;    // our server's sheet's revision last copied
  std::vector<std::pair<std::string, std::string>> q2_info;  // `info`'s pairs so far
  bool q2_match_asked = false;
};

// `serversheet <open|refresh|changed|close>` on the window's `r` at `now_s`.
void remote_sheet_command(Remote& r, const std::string& word, double now_s);
// One window step: our server's sheet copied, a stock QuakeWorld server's info read again, the
// listings asked again when due, and the listing lines taken out of `prints`.
void remote_sheet_step(Remote& r, std::vector<client::DemoPrint>& prints, double now_s);
// A print's lines the sheet takes (true) or leaves (false), per line; `keep` is what is left.
bool remote_sheet_take_print(RemoteSheet& s, bool q2, const std::string& text, std::string& keep);

}  // namespace shooters_edge::net
