// THE CLASSIC SERVERS' RESULTS (docs/accounts.md § The classic servers' results): a server the
// hub reads by its game's own status protocol (browse.hpp) reports no match to anyone, but its
// status answers show a match being played and then ending. The hub keeps each match it sees end
// as a result: the server, the map, the mode, when it ended and how long it ran, and the final
// scoreboard by name. Best effort and without accounts: a result counts in no player's totals and
// no ladder, a match the hub saw only part of (it started before the hub first heard the server)
// is kept with its length unknown, and one the hub missed the end of is lost.
//
// A match ends, as the status answers show it, when a record that was `playing` with two or more
// players scoring is followed by one in `intermission` (the scores are then the final ones), in
// `lobby` or `countdown` (the server went back to its warmup: the last playing scores stand), or by
// one on another map (the server moved on without an intermission). A playing record whose scores
// all dropped (a restart on the same map) ends it too.
//
//   results_see(r, server, name, before, playing_since_us, after, now_us, unix_s)   each new record
//   results_answer(spec, r, target, unix_s, out)   GET /results and /v1/results
//
// ge:summary The hub's results of classic servers' matches, seen ending in their status answers
// ge:keywords hub results classic adapter recent-stats match-end scoreboard results_see
// ge:keywords results-page v1-results best-effort without-accounts
// ge:skill net
// ge:test tests/hub_results_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "hub/api.hpp"
#include "hub/spec.hpp"
#include "net/address.hpp"
#include "net/status.hpp"

namespace ge::hub {

constexpr size_t kResultsKept = 200;  // the newest results kept; the oldest leave first
constexpr int kResultsShown = 50;     // a page of /results

struct ClassicResult {
  uint64_t id = 0;  // the hub's own count, from 1
  net::Address server;
  std::string name, map, mode;             // the server's name, the match's map and mode
  int64_t end_s = 0;                       // when the hub saw it end, Unix seconds
  int64_t length_s = -1;                   // how long it was played (-1: the hub saw only its end)
  std::vector<net::StatusPlayer> players;  // the players (no spectators), best score first
  std::vector<net::StatusTeam> teams;      // the teams, best first (none: every player alone)
};

struct Results {
  std::deque<ClassicResult> kept;  // oldest first
  uint64_t next_id = 1;
  uint64_t seen = 0;  // results ever taken
};

// A server's new record `after`, following `before` (its last record, playing since
// `playing_since_us` on the caller's clock, -1 unknown): true when it shows a match ending, which
// is kept as a result.
bool results_see(Results& r, const net::Address& server, const net::Status& before,
                 int64_t playing_since_us, const net::Status& after, int64_t now_us,
                 int64_t unix_s);

// The answer to GET `target` when its path is /results or /v1/results; false otherwise. `r` null:
// this hub reads no classic server.
bool results_answer(const HubSpec& spec, const Results* r, std::string_view target, int64_t unix_s,
                    Answer& out);

}  // namespace ge::hub
