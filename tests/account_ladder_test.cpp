// engine/account/ladder.hpp: a duel rates both accounts by Elo in tenths, the winner up by what
// the loser lost while both are new; a draw between equals moves nobody; a match with a bot, a
// guest, three a side or an unregistered pair is not rated; a duo is made only when both ask under
// one name, and its two-on-two matches rate it; a later season closes the last into the history
// with its best (enough matches only) and starts the season ratings again, the all-time ones not;
// the master's log read back gives the same ladders.
//
// ge:summary Test: the ranked ladders: Elo, rated matches, duos, seasons, champions, the log
// ge:keywords test ladder elo duo season champions history
// ge:skill net testing
#include "account/ladder.hpp"

#include <sstream>
#include <string>

#include "account/matches.hpp"
#include "check.hpp"

namespace ga = ge::account;
namespace gn = ge::net;

namespace {

const int64_t kStart =
    ga::kSeasonEpoch_s + int64_t{10} * 14 * 86400 + 3600;  // an hour into season 10

gn::ReportPlayer seat(uint64_t account, int64_t score, const char* team = "") {
  gn::ReportPlayer p;
  p.name = "p" + std::to_string(account);
  p.account = account;
  p.score = p.frags = score;
  p.team = team;
  return p;
}

gn::MatchReport duel(uint64_t a, int64_t sa, uint64_t b, int64_t sb, int64_t start) {
  gn::MatchReport r;
  r.game = "g";
  r.mode = "1on1";
  r.start_s = start;
  r.players = {seat(a, sa), seat(b, sb)};
  return r;
}

gn::MatchReport pairs(uint64_t a1, uint64_t a2, int64_t sa, uint64_t b1, uint64_t b2, int64_t sb,
                      int64_t start) {
  gn::MatchReport r;
  r.game = "g";
  r.mode = "2on2";
  r.start_s = start;
  r.teams = {{"red", sa, 4}, {"blue", sb, 13}};
  r.players = {seat(a1, sa / 2, "red"), seat(a2, sa - sa / 2, "red"), seat(b1, sb / 2, "blue"),
               seat(b2, sb - sb / 2, "blue")};
  return r;
}

const ga::Standing& at(const ga::Ladder& l, const char* mode, ga::LadderKind k, uint64_t who,
                       bool season = false) {
  const ga::LadderTable& t = l.tables.at({"g", mode, k});
  return (season ? t.season : t.all_time).at(who);
}

}  // namespace

TEST(ladder_a_duel_rates_both_by_elo) {
  ga::Ladder l;
  CHECK(ga::ladder_add(l, duel(1, 20, 2, 10, kStart)));
  const ga::Standing& w = at(l, "1on1", ga::LadderKind::duel, 1);
  const ga::Standing& lo = at(l, "1on1", ga::LadderKind::duel, 2);
  CHECK_MSG(w.rating == 15160 && lo.rating == 14840, std::to_string(w.rating));  // +-16.0 at K 32
  CHECK(w.wins == 1 && lo.losses == 1 && w.games == 1);
  CHECK(l.season == 10 && at(l, "1on1", ga::LadderKind::duel, 1, true).rating == 15160);
  // A draw between equals moves nobody.
  CHECK(ga::ladder_add(l, duel(3, 5, 4, 5, kStart)));
  CHECK(at(l, "1on1", ga::LadderKind::duel, 3).rating == 15000);
  CHECK(at(l, "1on1", ga::LadderKind::duel, 3).draws == 1);
  // The underdog's win moves more than the favourite's.
  CHECK(ga::ladder_add(l, duel(2, 30, 1, 10, kStart + 60)));
  CHECK(at(l, "1on1", ga::LadderKind::duel, 2).rating - 14840 > 160);
}

TEST(ladder_only_rated_matches_count) {
  ga::Ladder l;
  gn::MatchReport bot = duel(1, 20, 2, 10, kStart);
  bot.players[1].bot = true;
  CHECK(!ga::ladder_add(l, bot));
  CHECK(!ga::ladder_add(l, duel(1, 20, 0, 10, kStart)));  // a guest
  gn::MatchReport three = duel(1, 20, 2, 10, kStart);
  three.players.push_back(seat(3, 1));
  CHECK(!ga::ladder_add(l, three));  // a free-for-all of three
  CHECK_MSG(!ga::ladder_add(l, pairs(1, 2, 30, 3, 4, 20, kStart)), "no registered duos");
  CHECK(l.rated == 0 && l.tables.empty());
}

TEST(ladder_a_duo_is_made_when_both_ask) {
  ga::Ladder l;
  CHECK(ga::ladder_duo_ask(l, 1, 2, "Night Owls") == ga::DuoAnswer::asked);
  CHECK(ga::duo_of(l, 1, 2) == nullptr);
  CHECK(ga::ladder_duo_ask(l, 2, 1, "night owls") == ga::DuoAnswer::made);
  CHECK(ga::duo_of(l, 2, 1) != nullptr && ga::duo_of(l, 1, 2)->name == "Night Owls");
  CHECK(ga::ladder_duo_ask(l, 3, 4, "NIGHT OWLS") == ga::DuoAnswer::taken);
  CHECK(ga::ladder_duo_ask(l, 1, 2, "Night Owls") == ga::DuoAnswer::already);
  CHECK(ga::ladder_duo_ask(l, 3, 3, "Solo") == ga::DuoAnswer::self);
  CHECK(ga::ladder_duo_ask(l, 3, 4, "<b>") == ga::DuoAnswer::bad_name);
  CHECK(ga::ladder_duo_ask(l, 3, 4, "Day Larks") == ga::DuoAnswer::asked);
  CHECK(ga::ladder_duo_ask(l, 4, 3, "Day Larks") == ga::DuoAnswer::made);
  CHECK(ga::ladder_add(l, pairs(1, 2, 30, 4, 3, 20, kStart)));
  CHECK(at(l, "2on2", ga::LadderKind::pair, 1).wins == 1);
  CHECK(at(l, "2on2", ga::LadderKind::pair, 2).losses == 1);
  // A player may be in several duos; a pair mixing two duos' players is not rated.
  CHECK(ga::ladder_duo_ask(l, 1, 3, "Mixed") == ga::DuoAnswer::asked);
  CHECK(!ga::ladder_add(l, pairs(1, 3, 30, 2, 4, 20, kStart)));
}

TEST(ladder_seasons_close_into_history) {
  ga::Ladder l;
  for (int i = 0; i < 6; i++)
    CHECK(ga::ladder_add(l, duel(1, 20, 2, 10, kStart + int64_t{i} * 60)));
  for (int i = 0; i < 2; i++)
    CHECK(ga::ladder_add(l, duel(3, 20, 4, 10, kStart + int64_t{i} * 60)));
  const int64_t next = kStart + int64_t{14} * 86400;
  CHECK(ga::season_of(l, next) == 11);
  ga::ladder_close_due(l, next);
  CHECK(l.season == 11 && l.history.size() == 1);
  const ga::SeasonRecord& rec = l.history[0];
  CHECK(rec.season == 10 && rec.mode == "1on1" && rec.end_s - rec.start_s == int64_t{14} * 86400);
  CHECK_MSG(rec.champions.size() == 2, "players 3 and 4 played 2 matches: too few");
  CHECK(rec.champions[0].who == 1 && rec.champions[1].who == 2);
  CHECK(l.tables.at({"g", "1on1", ga::LadderKind::duel}).season.empty());
  CHECK(at(l, "1on1", ga::LadderKind::duel, 1).games == 6);  // all time kept
  // A late report from the closed season rates all time only.
  CHECK(ga::ladder_add(l, duel(2, 20, 1, 10, kStart + 7200)));
  CHECK(l.tables.at({"g", "1on1", ga::LadderKind::duel}).season.empty());
}

TEST(ladder_the_masters_log_gives_it_back) {
  ga::Matches m;
  gn::Key server{};
  server.fill(7);
  ga::matches_trust(m, server);
  CHECK(ga::matches_duo_ask(m, 1, 2, "Night Owls") == ga::DuoAnswer::asked);
  CHECK(ga::matches_duo_ask(m, 2, 1, "Night Owls") == ga::DuoAnswer::made);
  CHECK(ga::matches_duo_ask(m, 3, 4, "Day Larks") == ga::DuoAnswer::asked);
  CHECK(ga::matches_duo_ask(m, 4, 3, "Day Larks") == ga::DuoAnswer::made);
  for (uint64_t i = 0; i < 4; i++) {
    gn::MatchReport r = pairs(1, 2, 30, 3, 4, 20, kStart + static_cast<int64_t>(i) * 60);
    r.id = i + 1;
    ga::matches_add(m, server, r);
    gn::MatchReport d = duel(1, 20, 3, 10, kStart + static_cast<int64_t>(i) * 60);
    d.id = 100 + i;
    ga::matches_add(m, server, d);
  }
  CHECK(m.ladder.rated == 8);
  ga::Matches back;
  ga::matches_trust(back, server);
  std::istringstream lines(m.log);
  std::string line, error;
  while (std::getline(lines, line)) CHECK_MSG(ga::matches_line(back, line, error), error);
  CHECK(back.ladder.rated == 8 && back.ladder.duos.size() == 2);
  CHECK(at(back.ladder, "2on2", ga::LadderKind::pair, 1).rating ==
        at(m.ladder, "2on2", ga::LadderKind::pair, 1).rating);
  CHECK(at(back.ladder, "1on1", ga::LadderKind::duel, 3).rating ==
        at(m.ladder, "1on1", ga::LadderKind::duel, 3).rating);
}
