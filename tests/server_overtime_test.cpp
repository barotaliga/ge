// engine/server/overtime: a match tied at its time limit. The score compares the two best players
// or two teams (those who left counted for their team); extra time repeats while tied; sudden
// death ends at the next death; the tie-break at a gap of three (two when short); the golden frag
// at the first death that widens the gap noted before it; off, or a lead, ends the match.
//
// ge:summary Test: overtime at the time limit: the score, extra time, sudden death, tie-break,
// ge:summary golden frag
// ge:keywords test overtime extra-time sudden-death tie-break golden-frag match_score
// ge:skill net testing
#include "check.hpp"
#include "server/overtime.hpp"

using namespace ge::server;

namespace {

MatchScore two(int64_t a, int64_t b) { return {true, a, b}; }

OvertimeSettings mode(OvertimeMode m) {
  OvertimeSettings s;
  s.mode = m;
  return s;
}

}  // namespace

TEST(the_score_is_the_two_best_players_or_two_teams) {
  CHECK(!match_score({{"", 5, true}}, false).two);  // one player: nothing to compare
  const MatchScore duel =
      match_score({{"", 3, true}, {"", 9, true}, {"", 7, true}, {"", 20, false}}, false);
  CHECK(duel.two && duel.a == 9 && duel.b == 7);  // who left does not count without teams
  const MatchScore teams = match_score(
      {{"red", 3, true}, {"blue", 4, true}, {"red", 2, false}, {"blue", 1, true}}, true);
  CHECK(teams.two && teams.a == 5 && teams.b == 5);  // red's leaver kept
  CHECK(!match_score({{"red", 1, true}, {"blue", 1, true}, {"green", 1, true}}, true).two);
  CHECK(!match_score({{"red", 1, true}, {"blue", 1, false}}, true).two);  // one player present
  CHECK(overtime_mode(2) == OvertimeMode::sudden && overtime_mode(9) == OvertimeMode::off);
}

TEST(extra_time_repeats_while_tied_and_a_lead_ends_it) {
  Overtime o;
  OvertimeSettings s = mode(OvertimeMode::extra);
  s.minutes = 1;
  CHECK(overtime_time_up(o, s, two(4, 3)) == TimeUp::end);
  CHECK(o.said.empty());
  CHECK(overtime_time_up(o, s, two(4, 4)) == TimeUp::extend);
  CHECK(o.said.size() == 2 && o.said[0] == "Time is up and the game is a draw." &&
        o.said[1] == "1 minute of overtime follows.");
  CHECK(!overtime_died(o, s, two(5, 4)));  // extra time ends only on the clock
  CHECK(overtime_time_up(o, s, two(5, 5)) == TimeUp::extend);
  CHECK(o.periods == 2);
  CHECK(overtime_time_up(o, s, two(6, 5)) == TimeUp::end);
  CHECK(overtime_time_up(o, mode(OvertimeMode::off), two(1, 1)) == TimeUp::end);
}

TEST(sudden_death_ends_at_the_next_death) {
  Overtime o;
  const OvertimeSettings s = mode(OvertimeMode::sudden);
  CHECK(!overtime_died(o, s, two(2, 2)));  // before overtime: nothing ends
  CHECK(overtime_time_up(o, s, two(3, 3)) == TimeUp::no_clock);
  CHECK(o.said.back() == "Sudden death overtime begins.");
  CHECK(overtime_died(o, s, two(3, 2)));  // a suicide ends it too
}

TEST(the_tie_break_is_won_by_three_or_by_two_when_short) {
  Overtime o;
  OvertimeSettings s = mode(OvertimeMode::tiebreak);
  CHECK(overtime_time_up(o, s, two(5, 7)) == TimeUp::end);
  CHECK(overtime_time_up(o, s, two(5, 6)) == TimeUp::no_clock);
  CHECK(o.said.back() == "Tie-break overtime begins.");
  CHECK(!overtime_died(o, s, two(6, 6)));
  CHECK(!overtime_died(o, s, two(8, 6)));
  CHECK(overtime_died(o, s, two(9, 6)));
  Overtime shorter;
  s.short_gap = true;
  CHECK(overtime_time_up(shorter, s, two(1, 1)) == TimeUp::no_clock);
  CHECK(overtime_died(shorter, s, two(3, 1)));
}

TEST(the_golden_frag_ends_when_the_gap_widens) {
  Overtime o;
  const OvertimeSettings s = mode(OvertimeMode::golden);
  CHECK(!overtime_died(o, s, two(5, 2)));                        // gap 3 noted during the match
  CHECK(!overtime_died(o, s, two(5, 4)));                        // gap 1 noted
  CHECK(overtime_time_up(o, s, two(5, 4)) == TimeUp::no_clock);  // whatever the gap
  CHECK(o.said.back() == "Golden frag overtime begins.");
  CHECK(!overtime_died(o, s, two(5, 5)));  // gap 0: narrower, noted
  CHECK(overtime_died(o, s, two(6, 5)));   // gap 1 > 0: the golden frag
  overtime_reset(o);
  CHECK(o.playing == OvertimeMode::off && o.gap == -1);
}
