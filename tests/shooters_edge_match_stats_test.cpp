// Every classic game's match stats (games/shooters_edge/server/match_stats.hpp): QuakeWorld's and
// Quake II's columns from one layout; the counting rules (a hit proves its attack, splash and the
// rockets' damage, mid-airs, spawn frags, streaks and quad runs, items, damage by whom); and the
// same lines for both games, read off a report whose columns carry more after the stats'.
//
// ge:summary Tests the shared match stats: both games' columns, the counting rules, the lines
// ge:keywords match-stats shared-stats stats_died stats_lines midair spawn-frags quad-run
// ge:skill net shooter
#include <string>

#include "check.hpp"
#include "net/report.hpp"
#include "shooters_edge/net/qw_stats.hpp"
#include "shooters_edge/server/match_stats.hpp"
#include "shooters_edge/server/q2_stats.hpp"

namespace sv = shooters_edge::server;
namespace gn = ge::net;

TEST(both_games_lay_their_columns_out_alike) {
  const sv::StatsGame& qw = shooters_edge::net::qw_stats_game();
  const sv::StatsGame& q2 = sv::q2_stats_game();
  CHECK(sv::stats_columns(qw) == shooters_edge::net::kMatchStats);
  CHECK(sv::stats_columns(q2) == 4 * 11 + sv::kStatRestCount);
  CHECK(sv::stats_column_name(qw, 0) == "axe_attacks" &&
        sv::stats_column_name(q2, 0) == "bl_attacks");
  for (const sv::StatsGame* g : {&qw, &q2}) {
    CHECK(sv::stats_column_name(*g, sv::stat_rest(*g, sv::kStatRlSplash)) == "rl_splash");
    CHECK(sv::stats_column_name(*g, sv::stat_rest(*g, sv::kStatQuadRunMax)) == "quad_run_max");
    CHECK(sv::stats_column_name(*g, sv::stat_weapon(g->rl, sv::kStatHits)) == "rl_hits");
    CHECK(sv::stats_column_name(*g, sv::stats_columns(*g)).empty());
  }
  CHECK(sv::stats_column_name(qw, sv::stat_rest(qw, sv::kStatPowerC)) == "ring");
  CHECK(sv::stats_column_name(q2, sv::stat_rest(q2, sv::kStatArmourLight)) == "ja");
}

TEST(the_rules_count_alike_for_every_game) {
  const sv::StatsGame& g = sv::q2_stats_game();
  sv::PlayerStats a, b;
  const auto rest = [&](const sv::PlayerStats& p, sv::StatRest k) {
    return p.n[static_cast<size_t>(sv::stat_rest(g, k))];
  };
  const auto weapon = [&](const sv::PlayerStats& p, int w, int part) {
    return p.n[static_cast<size_t>(sv::stat_weapon(w, part))];
  };
  sv::stats_hit(&a, g.rl, 1);  // a hit the game did not see fired proves its attack
  CHECK(weapon(a, g.rl, sv::kStatAttacks) == 1);
  sv::stats_splash(g, &a, g.rl, 90);
  sv::stats_splash(g, &a, g.gl, 40);
  sv::stats_splash(g, &a, 0, 40);  // the blaster has no splash
  CHECK(rest(a, sv::kStatRlSplash) == 1 && rest(a, sv::kStatGlSplash) == 1);
  CHECK(rest(a, sv::kStatRlDamage) == 90);
  sv::stats_midair(g, &a, 120);
  sv::stats_midair(g, &a, 300);
  CHECK(rest(a, sv::kStatMidair) == 2 && rest(a, sv::kStatMidairHeight) == 420 &&
        rest(a, sv::kStatMidairHeightMax) == 300);
  sv::stats_blow(g, &a, &b, false, true, 50, true);
  sv::stats_blow(g, &a, &b, false, false, 7, false);
  sv::stats_blow(g, &a, &a, true, false, 9, false);
  CHECK(rest(a, sv::kStatDmgGiven) == 50 && rest(b, sv::kStatDmgTaken) == 50);
  CHECK(rest(a, sv::kStatDmgEwep) == 50 && rest(a, sv::kStatDmgTeam) == 7);
  CHECK(rest(a, sv::kStatDmgSelf) == 9);
  // Kills: a spawn frag, a streak of three, two of them under one quad, then a new quad.
  sv::stats_spawned(&b, 10);
  sv::stats_died(g, &b, &a, true, g.rl, false, 11);
  sv::stats_spawned(&b, 20);
  sv::stats_died(g, &b, &a, true, g.rl, true, 30);
  sv::stats_died(g, &b, &a, true, g.rl, true, 40);
  sv::stats_took(g, &a, sv::StatItem::quad);
  CHECK(rest(a, sv::kStatSpawnFrags) == 1 && weapon(a, g.rl, sv::kStatKills) == 3);
  CHECK(weapon(b, g.rl, sv::kStatDeaths) == 3 && rest(a, sv::kStatQuadRunMax) == 2);
  sv::stats_died(g, &a, nullptr, false, -1, false, 50);  // a suicide ends the streak
  CHECK(rest(a, sv::kStatStreakMax) == 3 && a.run == 0);
  sv::stats_quad_held(g, &a, 0.6);
  sv::stats_quad_held(g, &a, 0.6);
  CHECK(rest(a, sv::kStatQuadS) == 1);
  CHECK(sv::stats_row(g, a).size() == static_cast<size_t>(sv::stats_columns(g)));
}

TEST(both_games_show_the_same_lines) {
  const sv::StatsGame& g = sv::q2_stats_game();
  gn::MatchReport r;
  for (int k = 0; k < sv::stats_columns(g); k++) r.columns.push_back(sv::stats_column_name(g, k));
  r.columns.emplace_back("aim_spread");  // a column after the stats' is no matter
  gn::ReportPlayer p;
  p.name = "ana";
  p.team = "Wolves";
  p.score = 9;
  p.deaths = 3;
  sv::PlayerStats s;
  sv::stats_attack(&s, int(sv::SqWeapon::railgun), 20);
  sv::stats_hit(&s, int(sv::SqWeapon::railgun), 11);
  sv::stats_attack(&s, g.rl, 10);
  sv::stats_hit(&s, g.rl, 2);
  for (int i = 0; i < 5; i++) sv::stats_splash(g, &s, g.rl, 80);
  sv::stats_midair(g, &s, 150);
  sv::stats_took(g, &s, sv::StatItem::armour_heavy);
  p.columns = sv::stats_row(g, s);
  p.columns.push_back(0);
  r.players = {p};
  const std::vector<std::string> lines = sv::stats_lines(g, r);
  CHECK(lines.size() == 7);
  CHECK(lines[0] == "Player statistics:");
  CHECK_MSG(lines[1] == " ana [Wolves]: 9 frags, 3 deaths, 75.0% efficiency", lines[1]);
  CHECK_MSG(lines[2] == "  Weapons: rg 55.0%, rl 50.0% (5/10)", lines[2]);
  CHECK_MSG(lines[3] == "  Rockets: 2 direct, 80.0 average damage, 1 mid-air (highest 150)",
            lines[3]);
  CHECK_MSG(lines[4] ==
                "  Armour: jacket 0, combat 0, body 1, mega 0; quad 0, invulnerability 0, "
                "power armour 0",
            lines[4]);
  for (const std::string& l : lines)
    CHECK_MSG(l.find('`') == std::string::npos && l.find("next:") == std::string::npos, l);
  // Another game's columns are not these: nothing to show.
  CHECK(sv::stats_lines(shooters_edge::net::qw_stats_game(), r).empty());
}

TEST(the_top_scores_name_the_best_and_every_tie) {
  const sv::StatsGame& g = shooters_edge::net::qw_stats_game();
  gn::MatchReport r;
  for (int k = 0; k < sv::stats_columns(g); k++) r.columns.push_back(sv::stats_column_name(g, k));
  const auto player = [&](const char* name, int64_t score, int64_t deaths, int64_t streak) {
    gn::ReportPlayer p;
    p.name = name;
    p.score = score;
    p.deaths = deaths;
    sv::PlayerStats s;
    s.n[static_cast<size_t>(sv::stat_rest(g, sv::kStatStreakMax))] = streak;
    p.columns = sv::stats_row(g, s);
    return p;
  };
  r.players = {player("ana", 10, 5, 4), player("ben", 6, 2, 4)};
  const std::vector<std::string> top = sv::stats_top_lines(g, r);
  CHECK(top.size() == 4);
  CHECK(top[0] == "Top scores:");
  CHECK_MSG(top[1] == "  Frags: ana (10)", top[1]);
  CHECK_MSG(top[2] == "  Efficiency: ben (75.0%)", top[2]);
  CHECK_MSG(top[3] == "  Streak: ana, ben (4)", top[3]);
  r.players.pop_back();  // one player has no top scores
  CHECK(sv::stats_top_lines(g, r).empty());
}

TEST(the_stats_words_read_the_match_so_far_or_the_last) {
  const sv::StatsGame& g = sv::q2_stats_game();
  sv::StatsAsk ask;
  ask.me = "ana";
  std::string out;
  CHECK(!sv::stats_words(g, "say hi", ask, out));
  CHECK(sv::stats_words(g, "stats", ask, out));  // the warmup, and no match yet
  CHECK_MSG(out == "There are no stats yet: no match has been played.", out);
  gn::MatchReport last;
  for (int k = 0; k < sv::stats_columns(g); k++)
    last.columns.push_back(sv::stats_column_name(g, k));
  gn::ReportPlayer p;
  p.name = "ana";
  p.score = p.frags = 2;
  sv::PlayerStats s;
  sv::stats_attack(&s, g.rl, 4);
  sv::stats_hit(&s, g.rl, 1);
  p.columns = sv::stats_row(g, s);
  last.players = {p};
  ask.last = &last;
  ask.present = {"ana", "ben"};
  CHECK(sv::stats_words(g, "laststats", ask, out));  // a rocket counts by its splash
  CHECK_MSG(out.find("  Weapons: rl 0.0% (0/4)") != std::string::npos, out);
  CHECK(sv::stats_words(g, "stats ben", ask, out));  // on the server, never counted
  CHECK_MSG(out == "Nothing counted yet for ben.", out);
  CHECK(sv::stats_words(g, "stats zed", ask, out));
  CHECK_MSG(out == "There is no player zed in those stats.", out);
}
