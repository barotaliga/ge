// Every classic game's match stats, counted and shown; the rules in match_stats.hpp.
//
// ge:summary The shared stats' columns, the counting of each event and the lines a player reads
// ge:keywords stats_columns stats_column_name stats_died stats_took stats_player_lines stats_row
// ge:skill net shooter
// ge:test tests/shooters_edge_match_stats_test.cpp
#include "shooters_edge/server/match_stats.hpp"

#include <algorithm>

namespace shooters_edge::server {

namespace {

// A kill this soon after the victim came in is a spawn frag, s.
constexpr double kSpawnFrag = 2;

constexpr const char* kParts[4] = {"attacks", "hits", "kills", "deaths"};
constexpr const char* kRest[kStatRestCount] = {
    "rl_splash", "gl_splash",   "rl_damage",  "midair",      "midair_height", "midair_height_max",
    "dmg_given", "dmg_taken",   "dmg_team",   "dmg_self",    "dmg_ewep",      nullptr,
    nullptr,     nullptr,       nullptr,      nullptr,       nullptr,         nullptr,
    "quad_s",    "spawn_frags", "streak_max", "quad_run_max"};

bool weapon_of(const StatsGame& g, int w) { return w >= 0 && w < g.weapons; }

int64_t& col(PlayerStats& p, int c) { return p.n[static_cast<size_t>(c)]; }
int64_t& rest(const StatsGame& g, PlayerStats& p, StatRest r) { return col(p, stat_rest(g, r)); }

void close_runs(const StatsGame& g, PlayerStats& p) {
  int64_t& streak = rest(g, p, kStatStreakMax);
  int64_t& quad = rest(g, p, kStatQuadRunMax);
  streak = std::max(streak, p.run);
  quad = std::max(quad, p.quad_run);
  p.run = p.quad_run = 0;
}

// A hit proves its shot: one the game did not see fired is counted with it.
void proven(PlayerStats& p, int w) {
  int64_t& attacks = col(p, stat_weapon(w, kStatAttacks));
  attacks = std::max(attacks, col(p, stat_weapon(w, kStatHits)));
}

// A share as a percentage with one decimal ("45.5%").
std::string percent(int64_t part, int64_t whole_n) {
  const int64_t tenths = whole_n > 0 ? (part * 1000 + whole_n / 2) / whole_n : 0;
  return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "%";
}

std::string counted(int64_t n, const char* what) {
  return std::to_string(n) + " " + what + (n == 1 || n == -1 ? "" : "s");
}

// Whether the report's columns begin with the game's.
bool has_columns(const StatsGame& g, const ge::net::MatchReport& r) {
  const int n = stats_columns(g);
  if (r.columns.size() < static_cast<size_t>(n)) return false;
  for (int k = 0; k < n; k++)
    if (r.columns[static_cast<size_t>(k)] != stats_column_name(g, k)) return false;
  return true;
}

}  // namespace

int stats_columns(const StatsGame& g) { return 4 * g.weapons + kStatRestCount; }

std::string stats_column_name(const StatsGame& g, int column) {
  if (column < 0 || column >= stats_columns(g)) return {};
  if (column < 4 * g.weapons)
    return std::string(g.label[static_cast<size_t>(column / 4)]) + "_" + kParts[column % 4];
  const int r = column - 4 * g.weapons;
  if (r >= kStatArmourLight && r <= kStatPowerC)
    return g.item[static_cast<size_t>(r - kStatArmourLight)];
  return kRest[r];
}

void stats_spawned(PlayerStats* p, double now) {
  if (p != nullptr) p->spawned = now;
}

void stats_attack(PlayerStats* p, int weapon, int64_t n) {
  if (p == nullptr || weapon < 0 || weapon >= kStatWeaponsMost || n <= 0) return;
  col(*p, stat_weapon(weapon, kStatAttacks)) += n;
}

void stats_hit(PlayerStats* p, int weapon, int64_t n) {
  if (p == nullptr || weapon < 0 || weapon >= kStatWeaponsMost || n <= 0) return;
  col(*p, stat_weapon(weapon, kStatHits)) += n;
  proven(*p, weapon);
}

void stats_blow(const StatsGame& g, PlayerStats* by, PlayerStats* victim, bool self, bool enemy,
                int64_t points, bool big_gun) {
  if (self) {
    if (victim != nullptr) rest(g, *victim, kStatDmgSelf) += points;
  } else if (!enemy) {
    if (by != nullptr) rest(g, *by, kStatDmgTeam) += points;
  } else {
    if (by != nullptr) rest(g, *by, kStatDmgGiven) += points;
    if (victim != nullptr) rest(g, *victim, kStatDmgTaken) += points;
    if (by != nullptr && big_gun) rest(g, *by, kStatDmgEwep) += points;
  }
}

void stats_splash(const StatsGame& g, PlayerStats* by, int weapon, int64_t points) {
  if (by == nullptr || !weapon_of(g, weapon) || (weapon != g.rl && weapon != g.gl)) return;
  rest(g, *by, weapon == g.rl ? kStatRlSplash : kStatGlSplash)++;
  proven(*by, weapon);
  if (weapon == g.rl) rest(g, *by, kStatRlDamage) += points;
}

void stats_midair(const StatsGame& g, PlayerStats* by, int64_t height) {
  if (by == nullptr) return;
  rest(g, *by, kStatMidair)++;
  rest(g, *by, kStatMidairHeight) += height > 0 ? height : 0;
  int64_t& most = rest(g, *by, kStatMidairHeightMax);
  most = std::max(most, height);
}

void stats_died(const StatsGame& g, PlayerStats* victim, PlayerStats* killer, bool enemy,
                int weapon, bool quad, double now) {
  if (enemy && killer != nullptr) {
    if (weapon_of(g, weapon)) col(*killer, stat_weapon(weapon, kStatKills))++;
    if (victim != nullptr && now - victim->spawned <= kSpawnFrag)
      rest(g, *killer, kStatSpawnFrags)++;
    killer->run++;
    if (quad) killer->quad_run++;
  }
  if (enemy && weapon_of(g, weapon) && victim != nullptr)
    col(*victim, stat_weapon(weapon, kStatDeaths))++;
  if (victim != nullptr) close_runs(g, *victim);
}

void stats_took(const StatsGame& g, PlayerStats* p, StatItem item) {
  if (p == nullptr) return;
  rest(g, *p, static_cast<StatRest>(kStatArmourLight + static_cast<int>(item)))++;
  if (item == StatItem::quad) {  // a new quad: the last one's run is over
    int64_t& most = rest(g, *p, kStatQuadRunMax);
    most = std::max(most, p->quad_run);
    p->quad_run = 0;
  }
}

void stats_quad_held(const StatsGame& g, PlayerStats* p, double dt) {
  if (p == nullptr || dt <= 0) return;
  p->quad_carry += dt;
  const auto whole_s = static_cast<int64_t>(p->quad_carry);
  rest(g, *p, kStatQuadS) += whole_s;
  p->quad_carry -= static_cast<double>(whole_s);
}

void stats_close(const StatsGame& g, PlayerStats& p) { close_runs(g, p); }

std::vector<int64_t> stats_row(const StatsGame& g, const PlayerStats& p) {
  PlayerStats closed = p;
  close_runs(g, closed);
  return {closed.n.begin(), closed.n.begin() + stats_columns(g)};
}

std::vector<std::string> stats_player_lines(const StatsGame& g, const ge::net::MatchReport& r,
                                            const ge::net::ReportPlayer& p) {
  std::vector<std::string> out;
  if (!has_columns(g, r) || p.columns.size() < static_cast<size_t>(stats_columns(g))) return out;
  const auto v = [&](StatRest k) { return p.columns[static_cast<size_t>(stat_rest(g, k))]; };
  const auto w = [&](int weapon, int part) {
    return weapon_of(g, weapon) ? p.columns[static_cast<size_t>(stat_weapon(weapon, part))] : 0;
  };
  const int64_t effi_base = p.score + p.deaths;
  out.push_back(" " + p.name + (p.team.empty() ? "" : " [" + p.team + "]") + ": " +
                counted(p.score, "frag") + ", " + counted(p.deaths, "death") + ", " +
                percent(p.score > 0 ? p.score : 0, effi_base > 0 ? effi_base : 1) + " efficiency");
  std::string wp;
  const auto add = [&](const std::string& part) { wp += (wp.empty() ? "" : ", ") + part; };
  const int64_t rockets = w(g.rl, kStatAttacks);
  for (int i = 0; i < g.shown_count; i++) {
    const int sw = g.shown[static_cast<size_t>(i)];
    const int64_t attacks = w(sw, kStatAttacks);
    if (attacks <= 0) continue;
    const std::string label = g.label[static_cast<size_t>(sw)];
    if (sw == g.rl)
      add(label + " " + percent(v(kStatRlSplash), attacks) + " (" +
          std::to_string(v(kStatRlSplash)) + "/" + std::to_string(attacks) + ")");
    else if (sw == g.gl)
      add(label + " " + percent(v(kStatGlSplash), attacks));
    else
      add(label + " " + percent(w(sw, kStatHits), attacks));
  }
  if (!wp.empty()) out.push_back("  Weapons: " + wp);
  if (rockets > 0) {
    const int64_t splash = v(kStatRlSplash);
    const int64_t tenths = splash > 0 ? (v(kStatRlDamage) * 10 + splash / 2) / splash : 0;
    std::string line = "  Rockets: " + std::to_string(w(g.rl, kStatHits)) + " direct, " +
                       std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) +
                       " average damage";
    if (v(kStatMidair) > 0)
      line += ", " + std::to_string(v(kStatMidair)) + " mid-air (highest " +
              std::to_string(v(kStatMidairHeightMax)) + ")";
    out.push_back(line);
  }
  const auto item = [&](int k) {
    return std::string(g.words[static_cast<size_t>(k)]) + " " +
           std::to_string(v(static_cast<StatRest>(kStatArmourLight + k)));
  };
  out.push_back("  Armour: " + item(0) + ", " + item(1) + ", " + item(2) + ", " + item(3) + "; " +
                item(4) + ", " + item(5) + ", " + item(6));
  out.push_back("  Damage: taken " + std::to_string(v(kStatDmgTaken)) + ", given " +
                std::to_string(v(kStatDmgGiven)) + ", team " + std::to_string(v(kStatDmgTeam)) +
                ", self " + std::to_string(v(kStatDmgSelf)));
  out.push_back("  Streak " + std::to_string(v(kStatStreakMax)) + ", quad run " +
                std::to_string(v(kStatQuadRunMax)) + ", quad held " +
                std::to_string(v(kStatQuadS)) + "s, spawn frags " +
                std::to_string(v(kStatSpawnFrags)));
  return out;
}

std::vector<std::string> stats_lines(const StatsGame& g, const ge::net::MatchReport& r) {
  std::vector<std::string> out;
  if (!has_columns(g, r)) return out;
  out.emplace_back("Player statistics:");
  for (const ge::net::ReportPlayer& p : r.players)
    for (std::string& line : stats_player_lines(g, r, p)) out.push_back(std::move(line));
  for (std::string& line : stats_top_lines(g, r)) out.push_back(std::move(line));
  return out;
}

std::vector<std::string> stats_top_lines(const StatsGame& g, const ge::net::MatchReport& r) {
  std::vector<std::string> out;
  if (!has_columns(g, r) || r.players.size() < 2) return out;
  const auto n = static_cast<size_t>(stats_columns(g));
  // The best of one measure (in tenths for a share), every player who has it, when above 0.
  const auto top = [&](const char* what, const auto& of, bool tenths) {
    int64_t best = 0;
    std::string who;
    for (const ge::net::ReportPlayer& p : r.players) {
      if (p.columns.size() < n) continue;
      const int64_t v = of(p);
      if (v > best) {
        best = v;
        who = p.name;
      } else if (v == best && v > 0) {
        who += ", " + p.name;
      }
    }
    if (best <= 0) return;
    const std::string value =
        tenths ? std::to_string(best / 10) + "." + std::to_string(best % 10) + "%"
               : std::to_string(best);
    out.push_back("  " + std::string(what) + ": " + who + " (" + value + ")");
  };
  const auto rest = [&](StatRest k) {
    return [&g, k](const ge::net::ReportPlayer& p) {
      return p.columns[static_cast<size_t>(stat_rest(g, k))];
    };
  };
  top("Frags", [](const ge::net::ReportPlayer& p) { return p.score; }, false);
  top(
      "Efficiency",
      [](const ge::net::ReportPlayer& p) {
        const int64_t base = p.score + p.deaths, part = p.score > 0 ? p.score : 0;
        return base > 0 ? (part * 1000 + base / 2) / base : 0;
      },
      true);
  top("Damage given", rest(kStatDmgGiven), false);
  top("Mid-airs", rest(kStatMidair), false);
  top("Highest mid-air", rest(kStatMidairHeightMax), false);
  top("Streak", rest(kStatStreakMax), false);
  top("Quad run", rest(kStatQuadRunMax), false);
  top("Spawn frags", rest(kStatSpawnFrags), false);
  if (!out.empty()) out.insert(out.begin(), "Top scores:");
  return out;
}

}  // namespace shooters_edge::server
