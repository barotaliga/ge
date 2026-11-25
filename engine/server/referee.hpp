// THE MATCH'S REFEREE: a player who gives the server's administrator password in the game
// (`admin <password>`, or `referee <password>`) runs the match without a vote. The same for every
// game on the engine (docs/servers.md § The referee); the game hands it the password it keeps
// (a setting of its own), applies the acts it asks for, and prints what it says.
//
// BECOMING ONE: the right password makes the caller a referee, said to everyone ("<name> became
// a referee."); a wrong one is refused and logged; no password set: "There is no referee on this
// server."; the same word again gives it up. One try a second a slot. `acommands` lists the words.
//
// A REFEREE'S WORDS, each an act in `acts` for the game:
// - `endmatch` (and `break` during a match): the match ends now (`end_match`).
// - `tl <minutes>`: the time limit (`time_limit`, "Time limit set to <n>.").
// - `changemap <map>`: to that map now (`change_map`).
// - `kick <player>`, `kickban <player>` (banned 60 minutes), `ban <player> [minutes]` (60 by
//   default, 720 at most, never for good): `kick` or `ban` with the minutes. A referee is never
//   kicked, banned or muted.
// - `mute <player> [minutes]` (5 by default, 60 at most) and `unmute <player>`: a muted player's
//   chat is not passed on (`muted`), told "You are muted for N more minutes."
// - `shuffle`: the teams dealt again (`shuffle`).
// - `readyall` and `unreadyall`: every seat made ready or not (`ready_all`).
// A player is named by slot number, exact name, or the only name it begins.
//
// A GAME'S OWN WORDS (`RefereeWords`, handed in `RefereeCaller::words`; none: the ones above):
// the words it types for the book's acts and its login word, the wait between tries, how a player
// is named, a kick's reason and its ban, and every line the book says, so a classic game keeps
// its own (QuakeWorld's `admin <code>`, "<name> gains admins status!", `forcemap`, `forcebreak`).
// A game's table names only the words it has: any other line is not the book's (a mod's own
// `kick` or `ban` is left to the mod). `RefereeCaller::allowed` false: nobody may become one.
//
// A pure function of the acts in order on the server's clock (`now`, s): no wall clock.
//
// ge:summary The match's referee: the admin password in the game, then end, time limit, map,
// ge:summary kick, ban, mute and shuffle without a vote
// ge:keywords referee admin admin-password acommands endmatch break tl timelimit changemap
// ge:keywords kickban mute unmute shuffle readyall in-game-admin referee_words referee-words
// ge:keywords per-game-words forcemap forcebreak admin-code
// ge:skill net
// ge:test tests/server_referee_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "server/match.hpp"

namespace ge::server {

enum class RefereeActKind : uint8_t {
  end_match,
  time_limit,  // `value` minutes
  change_map,  // `text` the map
  kick,        // `slot`; `value` minutes banned after it (0: none), `text` the reason ("" none)
  ban,         // `slot` for `value` minutes
  shuffle,
  ready_all,  // `value` 1 ready, 0 not
};

struct RefereeAct {
  RefereeActKind kind = RefereeActKind::end_match;
  int slot = -1;
  double value = 0;
  std::string text;
};

struct Referee {
  uint32_t referees = 0;                          // a bit a slot
  std::array<double, kLobbySeats> muted_until{};  // server time a slot's chat is let through again
  std::array<double, kLobbySeats> tried{};        // the last password try, by slot
  std::vector<RefereeAct> acts;   // for the game to apply, in order (it clears them)
  std::vector<std::string> said;  // lines to print, in order (the game clears them)
  std::vector<int> to;            // each line's: -1 everyone, -2 the server's log, slot
};

// A game's words and lines (above). In a line, {name} is the caller, {player} the one acted on,
// {map} the map, {n} a number and {seconds} "second" or "seconds" by it, {reason} a kick's
// reason; a line with a line break is said as two lines. An empty line is not said.
struct RefereeWords {
  std::vector<std::string> login;                          // the words that make one, or unmake
  std::vector<std::pair<std::string, std::string>> words;  // a word typed, the book's word
  bool break_ends = true;             // a referee's `break` during a match ends it
  double wait_s = 1;                  // between two tries of the password, s
  bool wait_after_wrong = false;      // the wait follows only a wrong password (else any try)
  bool no_password_is_wrong = false;  // no password kept: every try wrong (else `none` said)
  bool exact_names = false;           // a player named by user id, then by his exact name alone
  bool kick_reason = false;           // the words after a kicked player's name are the reason
  double kick_ban_minutes = 0;        // a kicked player's address banned this long (0: not)
  std::string became, became_me;      // to everyone, and to the new referee
  std::string left, left_me;          // given up: to everyone, and to him
  std::string none;                   // nobody may become one (`allowed` false, or no password)
  std::string wrong, wrong_log;       // a wrong password: to him, and to the server's log
  std::string wait;                   // a try too soon ({n} the seconds left)
  std::string not_one;                // a referee's word from a player who is none
  std::string list;                   // `acommands`: the words
  std::string ended, no_match;        // `endmatch`; with no match on
  std::string map_usage, map_in_match, map_unknown, new_map;  // `changemap`
  std::string no_player, shielded;                            // no such player; a referee named
  std::string kicked, kicked_you, kick_said_reason;  // to everyone, to him, the reason's line
};
// The engine's own words (the header above): every game's but those with a table of their own.
const RefereeWords& referee_words();

// Who says a word: the slot, the password the server keeps ("" none), whether the match is on,
// the names of every slot ("" a free slot), and optionally the game's words, whether a referee
// is allowed at all, every slot's user id (a number names a player by it, not by the slot) and
// whether a map is on the server (none: every map name is taken).
struct RefereeCaller {
  int slot = -1;
  std::string_view password;
  bool in_match = false;
  const std::array<std::string, kLobbySeats>* names = nullptr;
  const RefereeWords* words = nullptr;
  bool allowed = true;
  const std::array<int, kLobbySeats>* ids = nullptr;
  bool (*map_known)(const void* ctx, std::string_view map) = nullptr;
  const void* map_ctx = nullptr;
};

bool referee_is(const Referee& r, int slot);
// A slot freed: its referee bit and mute gone.
void referee_left(Referee& r, int slot);
// Whether `slot`'s chat is held back at `now`; when it is, the line telling him why is said.
bool referee_muted(Referee& r, int slot, double now);

// The words above; false when the line is not the referee's (or the caller is no referee and the
// word is one only a referee has: then he is told so and it is true).
bool referee_command(Referee& r, const RefereeCaller& who, std::string_view line, double now);

}  // namespace ge::server
