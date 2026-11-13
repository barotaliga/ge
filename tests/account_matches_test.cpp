// The matches a master keeps (engine/account/matches.hpp): a report from a trusted server adds to
// its logged-in players' totals once however often it is sent; one from a server the owner has not
// listed is kept and counted for nobody; guests and bots add to nothing; the log reads back to the
// same totals.
//
// ge:summary Tests a master's match reports: trusted totals, reports sent again, the log
// ge:keywords match-report totals trusted-servers reports-log
// ge:skill net
#include <sstream>
#include <string>

#include "account/matches.hpp"
#include "check.hpp"

namespace ga = ge::account;
namespace gn = ge::net;

namespace {

gn::Key key(uint8_t b) {
  gn::Key k{};
  k.fill(b);
  return k;
}

gn::MatchReport duel(uint64_t id) {
  gn::MatchReport r;
  r.id = id;
  r.game = "id1";
  r.mode = "1on1";
  r.map = "arena";
  r.columns = {"rl_hits"};
  gn::ReportPlayer ann;
  ann.columns = {5};
  ann.name = "Ann";
  ann.account = 7;
  ann.score = ann.frags = 12;
  ann.deaths = 4;
  ann.seconds = 600;
  gn::ReportPlayer guest;
  guest.name = "visitor";
  guest.score = guest.frags = 4;
  guest.deaths = 12;
  guest.suicides = 1;
  guest.columns = {2};
  gn::ReportPlayer bot;
  bot.name = "bot1";
  bot.bot = true;
  bot.columns = {9};
  r.players = {ann, guest, bot};
  return r;
}

}  // namespace

TEST(matches_a_trusted_server_s_report_counts_once) {
  ga::Matches m;
  ga::matches_trust(m, key(1));
  CHECK(ga::matches_add(m, key(1), duel(1)) == ga::Taken::counted);
  CHECK(ga::matches_add(m, key(1), duel(1)) == ga::Taken::again);
  CHECK(ga::matches_add(m, key(1), duel(2)) == ga::Taken::counted);
  const ga::Totals t = ga::totals_sum(m, 7, {});
  CHECK(t.matches == 2 && t.wins == 2 && t.frags == 24 && t.deaths == 8 && t.seconds == 1200);
  CHECK(t.columns.at("rl_hits") == 10);
  CHECK(ga::totals_sum(m, 0, {}).matches == 0);  // a guest has no totals
  CHECK(m.kept.size() == 2 && m.again == 1);
}

TEST(matches_an_unlisted_server_s_report_is_kept_and_counts_for_nobody) {
  ga::Matches m;
  ga::matches_trust(m, key(1));
  CHECK(ga::matches_add(m, key(2), duel(1)) == ga::Taken::shown);
  CHECK(ga::totals_sum(m, 7, {}).matches == 0);
  CHECK(m.kept.size() == 1 && !m.kept.back().counted);
  // The same id from another server is another match.
  CHECK(ga::matches_add(m, key(1), duel(1)) == ga::Taken::counted);
}

TEST(matches_totals_by_map_and_mode_and_without_some_modes) {
  ga::Matches m;
  ga::matches_trust(m, key(1));
  gn::MatchReport a = duel(1), b = duel(2), c = duel(3);
  b.map = "dm3";
  c.map = "dm3";
  c.mode = "ffa";
  for (const gn::MatchReport& r : {a, b, c}) ga::matches_add(m, key(1), r);
  CHECK(ga::totals_sum(m, 7, {}).matches == 3);
  CHECK(ga::totals_sum(m, 7, {.map = "dm3"}).matches == 2);
  CHECK(ga::totals_sum(m, 7, {.modes = {"1on1"}}).matches == 2);
  CHECK(ga::totals_sum(m, 7, {.map = "dm3", .without = {"ffa"}}).matches == 1);
  CHECK(ga::totals_sum(m, 7, {.game = "other"}).matches == 0);
  CHECK(ga::totals_sum(m, 7, {.without = {"ffa"}}).columns.at("rl_hits") == 10);
}

TEST(matches_totals_at_a_good_ping_only) {
  ga::Matches m;
  ga::matches_trust(m, key(1));
  gn::MatchReport lan = duel(1), far = duel(2);
  lan.players[0].ping_avg = 12;
  far.players[0].ping_avg = 140;
  ga::matches_add(m, key(1), lan);
  ga::matches_add(m, key(1), far);
  CHECK(ga::ping_band(0) == 0 && ga::ping_band(40) == 1 && ga::ping_band(500) == 4);
  CHECK(ga::totals_sum(m, 7, {}).matches == 2);
  CHECK(ga::totals_sum(m, 7, {.ping_under = 40}).matches == 1);
  CHECK(ga::totals_sum(m, 7, {.ping_under = 200}).matches == 2);
}

TEST(matches_a_team_match_wins_for_the_best_team_and_the_log_reads_back) {
  ga::Matches m;
  ga::matches_trust(m, key(3));
  gn::MatchReport r = duel(9);
  r.mode = "2on2";
  r.teams = {{"red", 5, 4}, {"blue", 16, 13}};
  r.players[0].team = "red";
  r.players[1].team = "blue";
  ga::matches_add(m, key(3), r);
  ga::matches_add(m, key(4), duel(1));
  const ga::Totals t = ga::totals_sum(m, 7, {.modes = {"2on2"}});
  CHECK(t.wins == 0 && t.matches == 1);
  ga::Matches back;
  ga::matches_trust(back, key(3));
  std::istringstream lines(m.log);
  std::string line, error;
  int n = 0;
  while (std::getline(lines, line)) {
    CHECK_MSG(ga::matches_line(back, line, error), error);
    n++;
  }
  CHECK(n == 2);
  CHECK(back.totals == m.totals);
  CHECK(back.kept.size() == 2 && back.log.empty());
  CHECK(!ga::matches_line(back, "report zz 00", error));
}

TEST(matches_a_max_column_keeps_its_largest_and_the_rest_sum) {
  ga::Matches m;
  ga::matches_trust(m, key(1));
  gn::MatchReport a = duel(1), b = duel(2);
  a.columns = {"rl_hits", "streak_max"};
  b.columns = a.columns;
  a.players[0].columns = {5, 7};
  b.players[0].columns = {3, 4};
  b.map = "dm3";
  ga::matches_add(m, key(1), a);
  ga::matches_add(m, key(1), b);
  const ga::Totals t = ga::totals_sum(m, 7, {});
  CHECK(t.columns.at("rl_hits") == 8);
  CHECK(t.columns.at("streak_max") == 7);
}
