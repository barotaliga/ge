// AN ELECTION FOR A ROLE: a player asks to be a match's captain, coach or admin, the others answer
// `yes`, and when enough have said yes he has it. The same for every game on the engine
// (docs/servers.md § Elections and picks); the role is data (its name, a team), so ONE election is
// open on a server at a time whatever it is for, and the game gives the role when one passes.
//
// WHO VOTES: the game names the voters at each act (`voters`, a bit a slot: the seated people,
// never bots or spectators). The candidate need not be one of them (a coach watches). His own vote
// counts from the start when he is a voter. NEEDED is the round-up of `percent` (clamped 51..100)
// of the voters, at least `min_votes` (2): two voters must both agree, three need two, four three.
//
// THE ACTS:
// - `election_open`: refused while one is open ("busy") or while the candidate waits out the
//   block after his last one ended ("wait", the seconds left). Opened: "requested" to everyone,
//   "approve" to each other voter, "abort_hint" to him; then counted (it may pass at once).
// - `election_vote`: a voter's `yes` ("gives", then "more" while short; again: "still_good"; the
//   candidate: "not_self"); `no` withdraws a cast vote ("withdraws", then "more"), else nothing.
// - `election_abort`: the candidate's word again: "aborted"; `election_drop` when he leaves or the
//   map changes, silently. A voter who leaves takes his vote (`election_leave`), counted again.
// - `election_tick`: past `time_s` (60): "timed_out".
// Whenever one ends (passed, aborted, timed out) the candidate waits `block_s` (30) to ask again.
// A PASS sets `passed` (the role, the team and the slot) for the game to give and clear.
//
// THE LINES are the game's (`ElectionWords`): `{name}` the candidate or the voter, `{role}` the
// role, `{n}` a count, `{votes}` "vote" or "votes", `{seconds}` "second" or "seconds"; a "\n"
// splits a line in two and an empty line is not said. Each line is in `said`, beside it in `to`
// who it is for (-1 everyone).
//
// A pure function of the acts in order on the server's clock: no randomness.
//
// ge:summary One election for a role at a time: a captain, coach or admin by the players' yes
// ge:keywords election elect captain-election coach-election admin-election yes no k_vp_captain
// ge:keywords votes-needed election_open election_vote election_tick election-block
// ge:skill net
// ge:test tests/server_election_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ge::server {

constexpr int kElectionSeats = 32;

struct ElectionWords {
  std::string requested = "{name} has requested {role} status!";
  std::string approve = "Type yes in console to approve";
  std::string abort_hint = "Type {role} again to abort the election";
  std::string gives = "{name} gives a vote";
  std::string more = "{n} more {votes} needed";
  std::string still_good = "Your vote is still good";
  std::string not_self = "You cannot vote for yourself";
  std::string withdraws = "{name} withdraws a vote";
  std::string timed_out = "The voting has timed out.\nElection aborted";
  std::string aborted = "{name} aborts the election!";
  std::string busy = "An election is already in progress";
  std::string wait = "Wait {n} {seconds}!";
};

struct ElectionSettings {
  int percent = 51;     // of the voters (51..100)
  int min_votes = 2;    // the fewest yes that pass one
  double time_s = 60;   // an open one times out
  double block_s = 30;  // a candidate waits after his last one ended
};

// What an election is for, and who it is for: the game reads it back when one passes.
struct ElectionRole {
  std::string name;  // "captain", "coach", "admin": the game's word, said in the lines
  int team = -1;     // the team it is for, the game's own number (-1: none)
  int slot = -1;     // the candidate
};

struct Election {
  ElectionSettings settings;
  ElectionRole open;  // the one open (slot -1: none)
  double ends = 0;    // when it times out
  uint32_t yes = 0;   // a bit a voter who said yes
  std::array<double, kElectionSeats> blocked_until{};
  ElectionRole passed;            // set by a pass (slot -1: none); the game gives it and clears
  std::vector<std::string> said;  // lines for the game to print, in order
  std::vector<int> to;            // each line's: -1 everyone, else that slot
};

// The yes needed among `voters`: the round-up of the percent, at least min_votes.
int election_needed(const ElectionSettings& s, uint32_t voters);
bool election_running(const Election& e);
// Open one for `role` (its slot the candidate). False: refused (the line said).
bool election_open(Election& e, const ElectionWords& w, const ElectionRole& role,
                   const std::string& name, uint32_t voters, double now);
// A voter's yes (true) or no. False: no election is open (the word is someone else's).
bool election_vote(Election& e, const ElectionWords& w, int slot, const std::string& name, bool yes,
                   uint32_t voters, double now);
// The candidate calls it off: "aborted". False: he is not the candidate.
bool election_abort(Election& e, const ElectionWords& w, int slot, const std::string& name,
                    double now);
// Dropped silently (the candidate left, the map changed).
void election_drop(Election& e, double now);
// A slot left: the candidate's drops it, a voter's vote goes and the rest are counted again.
void election_leave(Election& e, const ElectionWords& w, int slot, uint32_t voters, double now);
// Once a frame: the timeout.
void election_tick(Election& e, const ElectionWords& w, double now);

}  // namespace ge::server
