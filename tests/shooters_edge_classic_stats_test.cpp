// A classic match's weapon stats (games/shooters_edge/net/classic_stats.hpp): every column named
// once; a bot match's report carries them, consistent with each other (no weapon hits more than
// it attacked, the damage given is the damage taken, kills by weapon no more than the deaths); and
// the lines the players read when it ends say it in a game's words.
//
// ge:summary Tests the classic match's weapon stats: the columns, a bot match's, the end lines
// ge:keywords weapon-stats match-stats stats_frame stats_lines midair splash
// ge:skill net shooter
#include <set>
#include <string>

#include "check.hpp"
#include "net/report.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_stats.hpp"

namespace se = shooters_edge::net;
namespace gn = ge::net;

namespace {

gn::MatchReport bot_match() {
  ge::server::ServerConfig c;
  c.hostname = "Stats Night";
  c.map = "arena";
  c.maxclients = 8;
  c.bots = 4;
  c.seed = 5;
  c.rules["timelimit"] = "0.5";
  ge::server::Server s;
  std::string error;
  CHECK_MSG(ge::server::server_open(s, se::classic_game(), c, nullptr, nullptr, error), error);
  for (int f = 0; f < 3500; f++) ge::server::server_frame(s);  // 45 s
  return s.reports.empty() ? gn::MatchReport{} : s.reports.back();
}

int64_t column(const gn::MatchReport& r, const gn::ReportPlayer& p, const std::string& name) {
  for (size_t i = 0; i < r.columns.size() && i < p.columns.size(); i++)
    if (r.columns[i] == name) return p.columns[i];
  return -1;
}

}  // namespace

TEST(classic_stats_every_column_is_named_once) {
  std::set<std::string> names;
  for (int k = 0; k < se::kMatchStats; k++) {
    const std::string n = se::stat_name(k);
    CHECK_MSG(!n.empty() && names.insert(n).second, "column " + std::to_string(k) + " " + n);
  }
  CHECK(se::kMatchStats <= static_cast<int>(gn::kReportColumns));
  CHECK(std::string(se::stat_name(se::stat_of(se::StatWeapon::rl, se::kMsHits))) == "rl_hits");
  CHECK(std::string(se::stat_name(se::stat_of(se::StatWeapon::axe, se::kMsAttacks))) ==
        "axe_attacks");
  CHECK(std::string(se::stat_name(se::kMsStreakMax)) == "streak_max");
  CHECK(std::string(se::stat_name(se::kMsMidairHeightMax)) == "midair_height_max");
}

TEST(classic_stats_a_bot_match_reports_them_consistently) {
  const gn::MatchReport r = bot_match();
  CHECK_MSG(!r.players.empty(), "the match left no report");
  CHECK(r.columns.size() == static_cast<size_t>(se::kMatchStats));
  int64_t attacks = 0, hits = 0, given = 0, taken = 0, kills = 0, deaths = 0;
  const char* weapons[] = {"axe", "sg", "ssg", "ng", "sng", "gl", "rl", "lg"};
  for (const gn::ReportPlayer& p : r.players) {
    CHECK(p.columns.size() == r.columns.size());
    for (const char* w : weapons) {
      const int64_t a = column(r, p, std::string(w) + "_attacks"),
                    h = column(r, p, std::string(w) + "_hits");
      CHECK_MSG(h >= 0 && h <= a,
                p.name + " " + w + " " + std::to_string(h) + "/" + std::to_string(a));
      attacks += a;
      hits += h;
      kills += column(r, p, std::string(w) + "_kills");
    }
    given += column(r, p, "dmg_given");
    taken += column(r, p, "dmg_taken");
    deaths += p.deaths;
  }
  CHECK_MSG(attacks > 0 && hits > 0,
            "the bots fought: " + std::to_string(hits) + "/" + std::to_string(attacks));
  CHECK_MSG(given == taken && given > 0,
            std::to_string(given) + " given, " + std::to_string(taken) + " taken");
  CHECK_MSG(kills > 0 && kills <= deaths,
            std::to_string(kills) + " kills by weapon, " + std::to_string(deaths) + " deaths");
}

TEST(classic_stats_the_end_lines_read_like_a_game) {
  gn::MatchReport r;
  for (int k = 0; k < se::kMatchStats; k++) r.columns.emplace_back(se::stat_name(k));
  gn::ReportPlayer p;
  p.name = "ann";
  p.score = p.frags = 12;
  p.deaths = 4;
  p.columns.assign(static_cast<size_t>(se::kMatchStats), 0);
  const auto set = [&](int k, int64_t v) { p.columns[static_cast<size_t>(k)] = v; };
  set(se::stat_of(se::StatWeapon::rl, se::kMsAttacks), 40);
  set(se::stat_of(se::StatWeapon::rl, se::kMsHits), 6);
  set(se::kMsRlSplash, 18);
  set(se::kMsRlDamage, 1350);
  set(se::kMsMidair, 2);
  set(se::kMsMidairHeightMax, 180);
  set(se::stat_of(se::StatWeapon::lg, se::kMsAttacks), 160);
  set(se::stat_of(se::StatWeapon::lg, se::kMsHits), 50);
  set(se::kMsRa, 3);
  set(se::kMsDmgGiven, 1500);
  set(se::kMsStreakMax, 5);
  r.players = {p};
  const std::vector<std::string> lines = se::stats_lines(r);
  CHECK(lines.size() == 7);
  CHECK(lines[0] == "Player statistics:");
  CHECK_MSG(lines[1] == " ann: 12 frags, 4 deaths, 75.0% efficiency", lines[1]);
  CHECK_MSG(lines[2] == "  Weapons: lg 31.3%, rl 45.0% (18/40)", lines[2]);
  CHECK_MSG(lines[3] == "  Rockets: 6 direct, 75.0 average damage, 2 mid-air (highest 180)",
            lines[3]);
  for (const std::string& l : lines)
    CHECK_MSG(l.find('`') == std::string::npos && l.find("next:") == std::string::npos, l);
  // A report without the weapon stats prints nothing.
  gn::MatchReport bare;
  bare.players = {p};
  CHECK(se::stats_lines(bare).empty());
}
