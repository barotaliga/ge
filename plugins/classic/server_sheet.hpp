// A CLASSIC SERVER'S SETTINGS SHEET READ FROM WHAT IT ALREADY SAYS: for a server that is not ours
// (engine/server/sheet.hpp's sheet comes from our own servers over the wire), the rows a client
// can fill from the server's info and its match mod's listings, each with the words that change
// it. So the server settings page shows a stock QuakeWorld or Quake II server's settings too
// (docs/servers.md § The settings sheet; porting/shooters-edge/server-settings-browse.md holds the
// facts this file is written from).
//
// QUAKEWORLD (`qw_sheet_from_info`): the server info a joined client holds names the map, the
// limits, the deathmatch mode, teamplay, the slots, the packet rate, the lag compensation and the
// movement switches. A setting at 0 is LEFT OUT of it, so a missing limit reads 0. The QuakeWorld
// MATCH MOD (its server info carries `ktxver`) adds `mode` (`1on1`, `2on2`, `4on4`, `ffa`, `ctf`...
// with `-midair`, `-instagib`, `-ra` and others after it for the modes on), and lets any player
// change its rules before a match by a word each, at once: `4on4`, `dmm3`, `tp` (cycles teamplay),
// `timeup1` and `timedown1`, `fragsup` and `fragsdown` (tens), `overtime` (cycles), `powerups`,
// `discharge`... Its votes are a word each that a player casts or takes back (`antilag`,
// `teamoverlay`, `nospecs`, `votecoop`), and a map is voted with `votemap <map>`; those rows are
// marked as votes. Its listings `status1` and `status2` print the rules the server info leaves
// out, a label and a value, the labels in coloured characters (`qw_match_listing_read` takes them,
// bit 7 cleared, into the sheet).
//
// QUAKE II (`q2_info_read`, `q2_sheet_from_info`): a joined client is not sent the server info;
// it asks with `info` (the server's own word, printed as a key padded to 20 columns and its value
// a line). The stock game has no vote and no setting a player may change, so its rows are shown
// fixed: the map, deathmatch, the limits, the slots, and the deathmatch flags a row a bit on their
// own page. NEVER send a stock Quake II server a word it does not know: its game says every unknown
// word as the player's chat. Quake II's MATCH MOD (`gamename` OpenTDM) votes with `vote <setting>
// <value>` and `yes` / `no`, and lists its rules with `settings` (a label ending in `:` and its
// value a line, `q2_match_listing_read`).
//
// ge:summary A classic server's settings sheet from its server info and its match mod's listings
// ge:keywords server-settings settings-sheet serverinfo match-mod mode status1 status2 votemap
// ge:keywords dmflags q2-info opentdm-settings server-settings-menu stock-server ktxver
// ge:skill classic net
// ge:test tests/classic_server_sheet_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "server/sheet.hpp"

namespace ge::classic {

// Which kind of server a sheet is read from (the listings to ask for depend on it).
enum class SheetSource : uint8_t { none, ours, qw_plain, qw_match, q2_plain, q2_match };

// A QuakeWorld server's kind from its server info (`ours`: it offered our own sheet).
SheetSource qw_sheet_source(const std::string& serverinfo, bool ours);
// The sheet's rows from a QuakeWorld server's info: every row it names (the values the server
// info leaves out read 0), and, for the match mod, its rules with their words, the ones only a
// listing says left with an empty value until `qw_match_listing_read` fills them.
void qw_sheet_from_info(ge::server::SettingsSheet& sheet, const std::string& serverinfo);
// The match mod's listing words a client sends to fill the rest, each a command line sent to the
// server as it stands ("status1", "status2"; a console's `cmd` adds no word of its own).
std::vector<std::string> qw_match_listing_asks();
// One printed line of a listing taken into the sheet (bit 7 of every byte cleared first); true
// when the line was one of a listing's (a window hides it then).
bool qw_match_listing_read(ge::server::SettingsSheet& sheet, std::string_view line);

// `info`'s printed lines read into key and value pairs (true when the line was one).
bool q2_info_read(std::string_view line, std::pair<std::string, std::string>& out);
SheetSource q2_sheet_source(const std::vector<std::pair<std::string, std::string>>& info);
// The sheet's rows from a Quake II server's `info`, for the stock game or the match mod.
void q2_sheet_from_info(ge::server::SettingsSheet& sheet,
                        const std::vector<std::pair<std::string, std::string>>& info);
// One printed line of the match mod's `settings` taken into the sheet (true when it was one).
bool q2_match_listing_read(ge::server::SettingsSheet& sheet, std::string_view line);

// The QuakeWorld match mod's `mode` without its suffixes ("1on1-midair": "1on1"), and whether a
// suffix is on ("midair").
std::string qw_mode_base(std::string_view mode);
bool qw_mode_has(std::string_view mode, std::string_view suffix);

}  // namespace ge::classic
