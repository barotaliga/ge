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
// A pure function of the acts in order on the server's clock (`now`, s): no wall clock.
//
// ge:summary The match's referee: the admin password in the game, then end, time limit, map,
// ge:summary kick, ban, mute and shuffle without a vote
// ge:keywords referee admin admin-password acommands endmatch break tl timelimit changemap
// ge:keywords kickban mute unmute shuffle readyall in-game-admin
// ge:skill net
// ge:test tests/server_referee_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "server/match.hpp"

namespace ge::server {

enum class RefereeActKind : uint8_t {
  end_match,
  time_limit,  // `value` minutes
  change_map,  // `text` the map
  kick,        // `slot`
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

// Who says a word: the slot, the password the server keeps ("" none), whether the match is on,
// and the names of every slot ("" a free slot).
struct RefereeCaller {
  int slot = -1;
  std::string_view password;
  bool in_match = false;
  const std::array<std::string, kLobbySeats>* names = nullptr;
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
