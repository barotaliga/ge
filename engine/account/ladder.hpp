// THE RANKED LADDERS (docs/accounts.md § The ladders): a chess-style rating for every account in
// one-on-one matches and for every registered duo in two-on-two matches, per game and mode, kept
// by the master from the trusted reports it counts (matches.hpp), and league seasons on top: every
// season (kSeasonDays by default) the season ratings start again, the season's best are kept as its
// champions, and the champions of every season are kept for good.
//
// A RATED MATCH is a counted report with two sides of one or two people each (two teams, or two
// players with no teams), every one of them logged in and none a bot. One a side rates the two
// accounts on the duel ladder of the report's game and mode; two a side rates the two duos on the
// pair ladder, and only when each side's two accounts are a registered duo. The side with the
// higher score wins; equal scores are a draw.
//
// THE RATING is Elo: a new entry starts at kRatingStart; after a match each side moves by K times
// (its result, 1, 0.5 or 0, less its expected result 1 / (1 + 10^((theirs - ours) / 400))), with K
// kKNew for its first kKNewGames matches and kKSettled after. Ratings are kept in tenths, so the
// sums stay exact. The all-time rating never starts again; the season's does.
//
// A DUO is two accounts and a name, made when each asks for the other under the same name
// (`ladder_duo_ask`), so nobody is put in a duo they did not ask for. A player may be in several
// duos with different partners; a name is one duo's alone.
//
// SEASONS are counted from kSeasonEpoch_s in `season_days` (the master's setting). A report
// starting in a later season, or `ladder_close_due` at a later time, closes the seasons behind it:
// each ladder's best (kChampionsKept, among those with kSeasonMinGames matches or more) are kept in
// `history` with the season's dates. Every duo line is appended to `log` (`duo <a> <b> <name hex>`)
// beside the reports, so reading the master's file back gives the same ladders and history.
//
// ge:summary The ranked ladders: Elo for one-on-one and registered duos, seasons, past champions
// ge:keywords ladder ranking rating elo duel duo two-on-two league season champions history
// ge:keywords ladder_add ladder_duo_ask ladder_close_due season_days
// ge:skill net
// ge:test tests/account_ladder_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "net/report.hpp"

namespace ge::account {

constexpr int64_t kRatingStart = 15000;  // tenths of a point: 1500.0
constexpr int64_t kKNew = 32, kKSettled = 16;
constexpr int64_t kKNewGames = 30;
constexpr int64_t kSeasonEpoch_s = 1767571200;  // 2026-01-05 00:00 UTC, a Monday
constexpr int kSeasonDaysDefault = 14;
constexpr int64_t kSeasonMinGames = 5;
constexpr size_t kChampionsKept = 3;
constexpr size_t kDuoName = 24;  // a duo's name, bytes

enum class LadderKind : uint8_t { duel, pair };
const char* ladder_kind_name(LadderKind k);

// One entry's standing: an account on a duel ladder, a duo on a pair ladder.
struct Standing {
  int64_t rating = kRatingStart;  // tenths
  int64_t games = 0, wins = 0, losses = 0, draws = 0;
  int64_t last_s = 0;  // the last rated match's start
};

struct Duo {
  uint64_t id = 0;        // 1 up, in the order made
  uint64_t a = 0, b = 0;  // the two accounts, a < b
  std::string name;
};

// A ladder's key: the game, the mode, the kind.
using LadderKey = std::tuple<std::string, std::string, LadderKind>;

struct LadderTable {
  std::map<uint64_t, Standing> all_time, season;  // by account (duel) or duo id (pair)
};

struct Champion {
  uint64_t who = 0;  // account or duo id
  int64_t rating = 0, games = 0, wins = 0;
};

struct SeasonRecord {
  std::string game, mode;
  LadderKind kind = LadderKind::duel;
  int64_t season = 0, start_s = 0, end_s = 0;
  std::vector<Champion> champions;  // best first
};

struct Ladder {
  int season_days = kSeasonDaysDefault;
  int64_t season = -1;  // the season the tables' season ratings are of (-1: none yet)
  std::map<LadderKey, LadderTable> tables;
  std::vector<Duo> duos;
  std::map<std::pair<uint64_t, uint64_t>, std::string> asks;  // asker, partner -> the name asked
  std::vector<SeasonRecord> history;                          // oldest first
  uint64_t rated = 0;
  std::string log;  // the duo lines since the master last wrote them
};

// The season a time falls in, and that season's first and last second + 1.
int64_t season_of(const Ladder& l, int64_t unix_s);
int64_t season_start(const Ladder& l, int64_t season);

enum class DuoAnswer : uint8_t {
  asked,     // waiting for the partner to ask under the same name
  made,      // both asked: the duo is made
  already,   // these two already are that duo
  taken,     // another duo has that name
  bad_name,  // empty, too long, or not letters, digits, spaces, dashes, dots or underscores
  self       // a duo of one
};
const char* duo_answer_words(DuoAnswer a);

DuoAnswer ladder_duo_ask(Ladder& l, uint64_t asker, uint64_t partner, std::string_view name);
// The duo of two accounts (either order), or null.
const Duo* duo_of(const Ladder& l, uint64_t a, uint64_t b);
const Duo* duo_by_id(const Ladder& l, uint64_t id);
// A counted report rated on its ladder when it is a rated match (true), else left alone.
bool ladder_add(Ladder& l, const net::MatchReport& r);
// Close every season before the one `unix_s` falls in.
void ladder_close_due(Ladder& l, int64_t unix_s);
// A duo line of the log read back; false with `error` naming what does not read.
bool ladder_line(Ladder& l, const std::string& line, std::string& error);
// A table's season standings, best first (only those with kSeasonMinGames or more ranked; the
// rest after, by rating).
std::vector<std::pair<uint64_t, Standing>> season_order(const LadderTable& t);
std::vector<std::pair<uint64_t, Standing>> all_time_order(const LadderTable& t);

}  // namespace ge::account
