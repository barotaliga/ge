// Tests: an election for a role (engine/server/election.hpp). The votes needed; the candidate's own
// vote; yes, a second yes, the candidate's yes and a no that withdraws; a pass hands the role
// over; one election at a time; the timeout; the block after one ends; a spectator candidate who
// is not a voter; a voter who leaves.
//
// ge:summary Tests: one election at a time, the votes needed, yes and no, timeout and block
// ge:keywords test election elect votes-needed captain-election coach-election
// ge:skill net testing
#include <string>

#include "check.hpp"
#include "server/election.hpp"

namespace sv = ge::server;

namespace {

bool said(const sv::Election& e, const std::string& part, int to = -2) {
  for (size_t i = 0; i < e.said.size(); i++)
    if (e.said[i].find(part) != std::string::npos && (to == -2 || e.to[i] == to)) return true;
  return false;
}

constexpr uint32_t seats(int n) { return (1u << n) - 1; }

}  // namespace

TEST(election_needs_a_majority_of_at_least_two) {
  sv::ElectionSettings s;
  CHECK(sv::election_needed(s, seats(1)) == 2);
  CHECK(sv::election_needed(s, seats(2)) == 2);
  CHECK(sv::election_needed(s, seats(3)) == 2);
  CHECK(sv::election_needed(s, seats(4)) == 3);
  CHECK(sv::election_needed(s, seats(8)) == 5);
  s.percent = 10;  // clamped to 51
  CHECK(sv::election_needed(s, seats(4)) == 3);
  s.percent = 100;
  CHECK(sv::election_needed(s, seats(4)) == 4);
}

TEST(election_passes_on_enough_yes_and_hands_the_role_over) {
  sv::Election e;
  sv::ElectionWords w;
  const uint32_t v = seats(4);
  CHECK(sv::election_open(e, w, {"captain", 1, 0}, "ana", v, 0));
  CHECK(said(e, "ana has requested captain status!", -1));
  CHECK(said(e, "Type yes in console to approve", 1));
  CHECK(!said(e, "Type yes in console to approve", 0));
  CHECK(said(e, "abort", 0));
  CHECK(sv::election_vote(e, w, 0, "ana", true, v, 1));
  CHECK(said(e, "You cannot vote for yourself", 0));
  CHECK(sv::election_vote(e, w, 1, "ben", true, v, 2));
  CHECK(said(e, "ben gives a vote"));
  CHECK(said(e, "1 more vote needed"));
  CHECK(sv::election_vote(e, w, 1, "ben", true, v, 3));
  CHECK(said(e, "Your vote is still good", 1));
  CHECK(sv::election_vote(e, w, 1, "ben", false, v, 4));
  CHECK(said(e, "ben withdraws a vote"));
  CHECK(sv::election_vote(e, w, 1, "ben", true, v, 5));
  CHECK(e.passed.slot == -1);
  CHECK(sv::election_vote(e, w, 2, "cid", true, v, 6));
  CHECK(e.passed.slot == 0);
  CHECK(e.passed.name == "captain");
  CHECK(e.passed.team == 1);
  CHECK(!sv::election_running(e));
  CHECK(!sv::election_vote(e, w, 3, "dee", true, v, 7));  // nothing open: not the election's
}

TEST(election_one_at_a_time_then_timeout_and_the_block) {
  sv::Election e;
  sv::ElectionWords w;
  const uint32_t v = seats(4);
  CHECK(sv::election_open(e, w, {"captain", -1, 0}, "ana", v, 0));
  CHECK(!sv::election_open(e, w, {"coach", 0, 1}, "ben", v, 1));
  CHECK(said(e, "An election is already in progress", 1));
  sv::election_tick(e, w, 59);
  CHECK(sv::election_running(e));
  sv::election_tick(e, w, 60);
  CHECK(said(e, "The voting has timed out."));
  CHECK(said(e, "Election aborted"));
  CHECK(!sv::election_running(e));
  CHECK(!sv::election_open(e, w, {"captain", -1, 0}, "ana", v, 70));
  CHECK(said(e, "Wait 20 seconds!", 0));
  CHECK(!sv::election_open(e, w, {"captain", -1, 0}, "ana", v, 89.5));
  CHECK(said(e, "Wait 1 second!", 0));
  CHECK(sv::election_open(e, w, {"captain", -1, 0}, "ana", v, 90));
  CHECK(sv::election_abort(e, w, 0, "ana", 91));
  CHECK(said(e, "ana aborts the election!"));
  CHECK(!sv::election_abort(e, w, 1, "ben", 92));
}

TEST(election_a_spectator_candidate_and_a_voter_who_leaves) {
  sv::Election e;
  sv::ElectionWords w;
  const uint32_t v = seats(3);  // slots 0..2 seated; the coach candidate watches from slot 5
  CHECK(sv::election_open(e, w, {"coach", 0, 5}, "eve", v, 0));
  CHECK(e.yes == 0);
  CHECK(sv::election_vote(e, w, 0, "ana", true, v, 1));
  CHECK(sv::election_vote(e, w, 1, "ben", true, v, 2));
  CHECK(e.passed.slot == 5);
  e.passed = {};
  sv::Election f;
  CHECK(sv::election_open(f, w, {"captain", -1, 0}, "ana", v, 0));
  CHECK(sv::election_vote(f, w, 1, "ben", false, v, 1));  // a no without a yes: nothing
  CHECK(!said(f, "withdraws"));
  sv::election_leave(f, w, 0, v, 2);
  CHECK(!sv::election_running(f));
  CHECK(f.passed.slot == -1);
}
