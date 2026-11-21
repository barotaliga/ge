// A MATCH'S NAME for its recording (`easyrecord`, the automatic recording of a match joined): a
// pattern per kind of match (`match_format_<kind>`) whose %-codes the match's facts fill.
//
// - **The kind**, the first that fits: no players: empty; game folder `qw` and the server info's
//   `race` naming the map: race; one player: solo; a Team Fortress game: tf duel with two players,
//   unknown with fewer than two teams, else tf clan war; a game folder holding `arena`: arena;
//   deathmatch 0: coop; two players: duel when teamplay is 0 or they are on two teams, else
//   unknown; teamplay 0 or no team over one player: ffa; more than two teams: multiteam, fewer:
//   unknown; two teams: their bigger one's size 2 is 2on2, 3 is 3on3, 4 is 4on4, else tdm. Teams
//   are the players' team words (an empty one a team, but never the bigger).
// - **The codes**: %n you (`match_name_nick` when set; spectating, `match_name_spec` after it),
//   %p player 1 (you when playing, else the first player), %e player 2 with two players (the
//   first other name, or the second player when watching) else team 2, %t team 1 (yours when
//   playing, else the first seen), %k and %l the teams' players joined by `match_name_and`, %O and
//   %E the teams' sizes (1 and the players less one without teamplay), %C the players, %a every
//   team's size joined by `match_name_on`, %b every team joined by `match_name_versus`, %v that
//   word, %T %F %N %D timelimit, fraglimit, teamplay and deathmatch, %M the map, %G the game
//   folder, %d %m %y %Y %H %Q %S day, month, year (two and four figures), hour, minute, second,
//   %% a %. Another code stays as written; a missing name is empty, a missing number 0.
// - **Cleaning**: each name is folded to plain letters and every character a file name cannot hold
//   (below space, space, `\ / ? * : < > " |`) becomes `_`, a run of them one `_`; then the whole
//   name the same way except that spaces stay and `/` makes folders (a `\` turns into `/`, a
//   leading `/` goes).
// - **The file**: `<name>_NNN.<ext>`, NNN the first of 000..999 whose name is free in each
//   extension asked about.
//
// ge:summary A match's recording name: its kind, the match_format_ patterns and their codes
// ge:keywords match-name match_format match_name easyrecord matchname matchtype duel 2on2 4on4
// ge:keywords tdm ffa coop race arena auto-record file-number
// ge:skill shooter replay
// ge:test tests/shooters_edge_match_name_test.cpp
#pragma once

#include <filesystem>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace shooters_edge::client {

struct NamedPlayer {
  std::string name, team;
  bool spectator = false;
};

struct MatchFacts {
  std::vector<NamedPlayer> players;  // in slot order, spectators among them
  int me = -1;                       // your slot (-1: unknown)
  bool spectating = false;
  bool race = false;      // the server info's `race` names the map
  bool fortress = false;  // a Team Fortress game
  std::string map, gamedir;
  int teamplay = 0, timelimit = 0, fraglimit = 0, deathmatch = 1;
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;  // local, at the start
};

struct MatchWords {
  std::string join = "_&_", versus = "_vs_", on = "on", nick, spec = "(SPEC)";
};

// The kind's word, as `$matchtype` says it: "" (empty), race, solo, tfduel, tfclanwar, arena,
// coop, duel, ffa, multiteam, 2on2, 3on3, 4on4, tdm or unknown.
std::string match_type(const MatchFacts& f);
// The pattern setting a kind reads ("match_format_duel"; "" for empty and unknown).
std::string match_format_setting(std::string_view type);
// The pattern filled in, uncleaned.
std::string expand_match_name(std::string_view pattern, const MatchFacts& f, const MatchWords& w);
// A name's characters made fit for a file (above): `path` keeps spaces and folders.
std::string clean_match_text(std::string_view text, bool path);
// The match's whole name: its kind's pattern (`pattern_of` the setting's value) filled and cleaned.
std::string match_name(const MatchFacts& f, const MatchWords& w,
                       const std::function<std::string(const std::string& setting)>& pattern_of);
// `<name>_NNN` with the first number free in `folder` for every one of `exts` ("" when all 1000
// are taken).
std::string free_match_file(const std::filesystem::path& folder, const std::string& name,
                            std::initializer_list<const char*> exts);

// The patterns (`match_format_*`), the words (`match_name_*`) and the automatic recording's
// settings (`match_auto_record`, `match_auto_minlength`, `match_auto_spectating`), in the classic
// layer.
void add_match_cvars(ge::console::Console& con);
// The words as the settings hold them.
MatchWords match_words(const ge::console::Cvars& cvars);

}  // namespace shooters_edge::client
