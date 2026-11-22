// A SERVER'S SETTINGS SHEET: what a player may read and change of the server he is on, for any
// game: the map, the mode, the limits, the rules and the player aids, each with its value now and
// HOW it changes (docs/servers.md § The settings sheet). A client's menu shows it (Shooter's Edge's
// SETTINGS ▸ SERVER SETTINGS), so nobody has to know the commands to see how a server is set up.
//
// A ROW is one setting: the name it is known by (a cvar, or a key of the game's own such as
// `speedjump:3` for one player's grant), the words a player reads ("Frag limit"), the page it is
// on (`group`: "" the first page, else a subpage's words such as "Player help"), its value now,
// what each value does (`values`, the console's grammar: console/words.hpp, "0: no limit; 1..N:
// that many frags"), one sentence of help, and how it changes:
//
// - `fixed`: shown, not changed from here (set when the server starts, or by its owner alone);
// - `direct`: any player may change it with the row's line, at once;
// - `vote`: changing it calls a vote (engine/server/vote.hpp, or a match mod's own), which the
//   other players must pass; a menu marks it (`sheet_mark`: an asterisk) and asks before calling;
// - `admin`: only an administrator may change it (rcon or a login), shown so he can.
//
// THE LINE that changes a row is the console line a client sends the server with `%v` standing
// for the value (`sheet_change_line`): by default `vote <name> %v` for a vote and `<name> %v` for
// the rest; a game or a reader of another server's words names its own (`vote speedjump 3 %v`,
// a match mod's `timelimit %v`); a line with no `%v` is sent as it is (a word that turns a
// setting, or casts a vote on it, by itself: `antilag`). A server whose words only STEP a setting
// (a match mod's `fragsup` and `fragsdown`, or a `tp` that cycles) gives `<up>|<down>` (`<up>|` for
// one that cycles): `sheet_change_lines` sends the up line once a choice from the value now to the
// one asked, or the down line once a choice back (a cycle wraps round forward).
//
// THE WIRE (any game's own message carrying a part number and a text): `begin` drops the rows
// held and starts a new sheet, each `row` puts one (a row of a name held replaces it, so a change
// later is one `row`), `end` says the sheet is whole. A row's text is an info string
// (`\n\fraglimit\w\Frag limit\v\20...`) whose keys are one letter; a backslash in a field is
// written `/`. `sheet_row_text` drops the help and then the values when the text would pass
// `most` bytes, so a row always fits a message.
//
// CHOICES for a menu (`sheet_choices`): each literal value the grammar names, with its meaning,
// then, for a number range, a ladder of round numbers inside it (1, 2, 3, 5, 10, 15, 20, 30...),
// and the value now when it is none of those. A row whose values are any text (`TEXT`) or any
// number with no range has no choices but its value now: it is shown, and changed by typing.
//
// ge:summary A server's settings sheet: rows of value, words, group, how each changes; its wire
// ge:keywords server-settings settings-sheet sheet browse-settings vote-mark votable admin-only
// ge:keywords sheet_row_text sheet_row_read sheet_take sheet_choices sheet_change_line
// ge:keywords server-settings-menu player-help match-mode
// ge:skill net
// ge:test tests/server_sheet_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "server/vote.hpp"

namespace ge::server {

enum class SheetHow : uint8_t { fixed, direct, vote, admin };
// "fixed", "direct", "vote", "admin"; and back (fixed for any other word).
const char* sheet_how_name(SheetHow how);
SheetHow sheet_how_named(std::string_view word);
// What a menu writes after a row's words: "*" for a vote, "" else.
const char* sheet_mark(SheetHow how);

struct SheetRow {
  std::string name = {};    // what it is known by: a cvar, or a key of the game's own
  std::string words = {};   // what a player reads
  std::string group = {};   // the page it is on: "" the first, else the subpage's words
  std::string value = {};   // now
  std::string values = {};  // what each value does (console/words.hpp), "" any
  std::string help = {};    // one sentence
  std::string line = {};    // the line that changes it, `%v` the value ("" the default for `how`)
  SheetHow how = SheetHow::fixed;
};

struct SettingsSheet {
  std::vector<SheetRow> rows;  // in the order told
  bool whole = false;          // the server said every row (`end`)
  bool asked = false;          // the client asked for it and waits
  uint32_t revision = 0;       // bumped by every change, so a menu knows to refresh
};

enum class SheetPart : uint8_t { begin = 0, row = 1, end = 2 };

// A row as its wire text, at most `most` bytes (the help, then the values, dropped to fit; 0: no
// cap); and read back (false when the text names no row).
std::string sheet_row_text(const SheetRow& row, size_t most = 0);
bool sheet_row_read(std::string_view text, SheetRow& out);

// One part of the wire taken into `sheet` (an unknown part number is ignored).
void sheet_take(SettingsSheet& sheet, int part, std::string_view text);
// A row put in: one of the same name replaced where it stands, else added at the end.
void sheet_put(SettingsSheet& sheet, SheetRow row);
const SheetRow* sheet_find(const SettingsSheet& sheet, std::string_view name);
// The subpages' words in the order their first rows came ("" left out).
std::vector<std::string> sheet_groups(const SettingsSheet& sheet);

// The line that sets `row` to `value` (`%v` replaced; a value with a space is quoted).
std::string sheet_change_line(const SheetRow& row, std::string_view value);
// Every line that sets `row` to `value`: the one line, or a stepping row's steps (sheet.hpp's
// THE LINE); empty when the value now or the one asked is not among the choices of a stepping
// row, or is the value now.
std::vector<std::string> sheet_change_lines(const SheetRow& row, std::string_view value);

struct SheetChoice {
  std::string value;
  std::string meaning;  // the grammar's words for it, "" for a number of a range
};
std::vector<SheetChoice> sheet_choices(const SheetRow& row);
// The meaning the grammar gives `value` ("" none).
std::string sheet_meaning(const SheetRow& row, std::string_view value);

// How a server on the vote's lists (vote.hpp) changes `name`: by vote when the lists let it be
// voted on and votes are on, else fixed.
SheetHow sheet_how_by_vote(const VoteSettings& settings, std::string_view name);

// The words a row shows when its server gives none: the name without its family's prefix (`sv_`,
// `pm_`, `k_`, `g_`, `cl_`), underscores as spaces, the first letter a capital; the common
// settings by their usual words ("timelimit": "Time limit", "fraglimit": "Frag limit").
std::string sheet_words_of(std::string_view name);

}  // namespace ge::server
