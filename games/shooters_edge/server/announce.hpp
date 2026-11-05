// THE LOBBY'S WORDS ON THE WIRE: what the players read and hear of the lobby (lobby.hpp) as the
// server's own events (messages.hpp), in a loaded classic game's words when its profile gives them
// (profiles/words.hpp, read from the server's local info), else the lobby's own lines.
//
// - THE LINES: each of the lobby's lines since the last call (`said`, with `notes` saying what each
//   is about). In a game's words: `ready` / `ready_team` (a team game), `unready`, `stops` (a break
//   that stopped the countdown, said in place of `unready`), `all_ready` then `timer` when everyone
//   is ready, `timer` alone when the leader starts it, `aborting` when anything else stops it,
//   `begun` with the centre print cleared when the match begins, `ready_again` to a ready player
//   alone; joining, leaving, team moves and the return to warmup say nothing (the mod tells of
//   arrivals; a team move is the player's own business). A refusal and an answer (`team`, the
//   roster) go to that player alone, in the lobby's words. Without a game's words every line is a
//   print to everyone at the high level, as the lobby said it.
// - THE COUNTDOWN, in a game's words only: each second it shows a new number (the first at once) a
//   centre print to everyone: `count_head` with the seconds, then the match's settings a line each
//   (`count_dm`, `count_mode` with `mode_team` in a team game, `mode_duel` for two players,
//   `mode_ffa` else, `count_antilag` when antilag is on, `count_teamplay` in a team game,
//   `count_timelimit` and `count_fraglimit` when set, `count_powerups`), then `count_tail`. Its
//   start plays `sound_timer` to everyone, and each of its last five seconds `sound_tick`, with
//   `get_ready` to every seat not ready.
//
// The events wait in `pending` until `announce_flush` hands them to the rules that are current
// (after a map change, the new map's), so the match's first words reach the match. The host also
// keeps the phase in the local info (`announce_phase`) for the mod.
//
// ge:summary The lobby's words as server events: ready, break, the countdown's centre print, begun
// ge:keywords announce announcer lobby-words countdown centerprint centre-print ready break
// ge:keywords all-players-ready timer-started match-has-begun get-ready match-look classic-words
// ge:skill shooter net
// ge:test tests/shooters_edge_words_test.cpp
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "shooters_edge/server/lobby.hpp"
#include "shooters_edge/server/messages.hpp"

namespace shooters_edge::server {

// The match's settings the countdown shows (the configured ones: warmup holds the limits at 0).
struct MatchLook {
  int deathmatch = 1;
  int teamplay = 0;
  int timelimit = 0;
  int fraglimit = 0;
  int antilag = 0;
  bool powerups = true;
};

// `look` with each `<setting> <value>` line of `lines` applied (deathmatch, teamplay, timelimit,
// fraglimit, sv_antilag; quotes allowed; anything else passed over), the last one winning.
MatchLook match_look(MatchLook look, const std::vector<std::string>& lines);

struct Announcer {
  int shown = 0;                  // the countdown's second last shown (0: none)
  std::vector<NetEvent> pending;  // made, not yet handed to the rules
};

using Info = std::vector<std::pair<std::string, std::string>>;

// The lobby's lines since the last call (`said` and `notes` emptied) and the countdown's second at
// `now`, as events in `pending`; `info` the server's local info the game's words are read from.
void announce(Announcer& a, Lobby& l, const Info& info, const MatchLook& look, double now);
// The lobby's phase in the server's local info as `ge_phase` (warmup, countdown or match), where
// the mod reads it (a classic game's `quiet_warmup`).
void announce_phase(Info& info, LobbyPhase phase);
// The pending events appended to `out` (stamped `now`), `pending` emptied.
void announce_flush(Announcer& a, MessageSink& out, double now);

}  // namespace shooters_edge::server
