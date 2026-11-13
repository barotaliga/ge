// A classic match's tally and report; the rules in classic_report.hpp.
//
// ge:summary The tally read off the rules' world each frame, and the report built from it
// ge:keywords match-report match-tally classic-report
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_report_test.cpp
#include "shooters_edge/net/classic_report.hpp"

#include <algorithm>
#include <climits>

#include <cstdlib>
#include "core/num.hpp"

namespace shooters_edge::net {

namespace sv = shooters_edge::server;
namespace gn = ge::net;

namespace {

std::string info(const sv::WorldClient& c, const char* key) {
  for (const auto& [k, v] : c.userinfo)
    if (k == key) return v;
  return {};
}

bool counted(const sv::WorldClient& c) { return c.active && !c.spectator; }

void seat_start(TallySeat& s, const sv::WorldClient& c, double now, uint64_t account) {
  s = {};
  s.on = true;
  s.userid = c.userid;
  s.bot = c.bot;
  s.name = c.name;
  s.team = info(c, "team");
  s.account = account;
  s.since = s.until = now;
  s.base = s.frags = static_cast<int64_t>(c.numbers.frags);
  s.alive = c.numbers.health > 0;
}

void tally_reset(MatchTally& t, const sv::RulesWorld& w, bool counting,
                 const std::array<uint64_t, ge::shooter::kMaxClients>& accounts) {
  t.seats = {};
  t.left.clear();
  t.map = w.map_name;
  t.start = t.last = t.scored = w.time;
  t.counting = counting;
  t.ended = false;
  if (!counting) return;
  for (size_t i = 0; i < t.seats.size(); i++)
    if (counted(w.clients[i])) seat_start(t.seats[i], w.clients[i], w.time, accounts[i]);
}

int64_t played_s(const TallySeat& s, double end) {
  return static_cast<int64_t>(
      ge::core::num::round(std::max(0.0, std::min(s.until, end) - s.since)));
}

gn::ReportPlayer player_of(const TallySeat& s, bool teams, double end) {
  gn::ReportPlayer p;
  p.name = s.name;
  if (teams) p.team = s.team;
  p.account = s.account;
  p.bot = s.bot;
  p.frags = s.frags - s.base;
  p.score = p.frags;
  p.deaths = s.deaths;
  p.suicides = s.suicides;
  p.team_kills = s.team_kills;
  p.seconds = played_s(s, end);
  if (s.ping_n > 0) {
    p.ping_min = s.ping_min;
    p.ping_max = s.ping_max;
    p.ping_avg = (s.ping_sum + s.ping_n / 2) / s.ping_n;
  }
  return p;
}

}  // namespace

const char* classic_mode(bool teamplay, bool coop, int seats) {
  if (coop) return "coop";
  if (teamplay) return seats <= 4 ? "2on2" : "4on4";
  return seats <= 2 ? "1on1" : "ffa";
}

void tally_frame(MatchTally& t, const sv::RulesWorld& w, bool counting,
                 const std::array<uint64_t, ge::shooter::kMaxClients>& accounts) {
  const bool fresh = w.map_name != t.map || w.time < t.last;
  if (fresh || (!t.ended && counting != t.counting)) {
    tally_reset(t, w, counting, accounts);
    return;
  }
  t.last = w.time;
  if (t.ended || !counting) return;
  for (size_t i = 0; i < t.seats.size(); i++) {
    TallySeat& s = t.seats[i];
    const sv::WorldClient& c = w.clients[i];
    if (s.on &&
        (!counted(c) || c.userid != s.userid)) {  // the player left (or another took the slot)
      t.left.push_back(std::move(s));
      s = {};
    }
    if (!counted(c)) continue;
    if (!s.on) {
      seat_start(s, c, w.time, accounts[i]);
      continue;
    }
    const auto frags = static_cast<int64_t>(c.numbers.frags);
    const bool alive = c.numbers.health > 0;
    if (s.alive && !alive) {
      s.deaths++;
      if (frags < s.frags) s.suicides++;
    } else if (alive && frags < s.frags) {
      s.team_kills++;
    }
    s.alive = alive;
    if (frags != s.frags) t.scored = w.time;
    s.frags = frags;
    s.until = w.time;
    const int64_t ping = c.ping_ms;
    s.ping_min = s.ping_n == 0 ? ping : std::min(s.ping_min, ping);
    s.ping_max = std::max(s.ping_max, ping);
    s.ping_sum += ping;
    s.ping_n++;
    s.name = c.name;
    s.team = info(c, "team");
    if (accounts[i] != 0) s.account = accounts[i];
  }
}

bool tally_end(MatchTally& t, const sv::RulesWorld& w, const TallyFacts& facts,
               gn::MatchReport& out) {
  if (t.ended || !t.counting || t.map != w.map_name) return false;
  t.ended = true;
  // Where it ended: the time limit's moment, or the frag that reached the frag limit.
  int64_t best = INT64_MIN;
  int present = 0;
  for (const TallySeat& s : t.left) best = std::max(best, s.frags - s.base);
  for (const TallySeat& s : t.seats)
    if (s.on) {
      best = std::max(best, s.frags - s.base);
      present++;
    }
  if (present == 0 && t.left.empty()) return false;
  const bool frag_limit = facts.fraglimit > 0 && static_cast<double>(best) >= facts.fraglimit;
  const double time_end = facts.timelimit * 60.0;  // the rules count it from the map's start
  const bool time_limit = facts.timelimit > 0 && w.time >= time_end;
  const double end = frag_limit ? t.scored : time_limit ? std::max(t.start, time_end) : w.time;
  std::vector<gn::ReportPlayer> players;
  players.reserve(t.left.size() + static_cast<size_t>(present));
  for (const TallySeat& s : t.left) players.push_back(player_of(s, facts.teams, end));
  for (const TallySeat& s : t.seats)
    if (s.on) players.push_back(player_of(s, facts.teams, end));
  // The best first; a tie by the fewer deaths, then by name, so the order is the same everywhere.
  std::stable_sort(players.begin(), players.end(), [](const auto& a, const auto& b) {
    if (a.score != b.score) return a.score > b.score;
    if (a.deaths != b.deaths) return a.deaths < b.deaths;
    return a.name < b.name;
  });
  out = {};
  out.mode = facts.mode;
  out.map = w.map_name;
  out.duration_ms = static_cast<int64_t>(ge::core::num::round((end - t.start) * 1000));
  if (facts.teams) {
    for (const gn::ReportPlayer& p : players) {
      if (p.team.empty()) continue;
      auto it = std::find_if(out.teams.begin(), out.teams.end(),
                             [&](const gn::ReportTeam& team) { return team.name == p.team; });
      if (it == out.teams.end()) {
        out.teams.push_back({p.team, 0, 0});
        it = out.teams.end() - 1;
      }
      it->score += p.score;
    }
    for (gn::ReportTeam& team : out.teams)  // a team's colour: its first player's still here
      for (size_t i = 0; i < t.seats.size(); i++)
        if (t.seats[i].on && t.seats[i].team == team.name) {
          team.colour = static_cast<uint32_t>(std::atoi(info(w.clients[i], "bottomcolor").c_str()));
          break;
        }
    std::stable_sort(out.teams.begin(), out.teams.end(),
                     [](const auto& a, const auto& b) { return a.score > b.score; });
  }
  // How it ended: a limit reached, else a side gone, else broken off.
  const size_t sides_needed = 2;
  size_t sides = facts.teams ? 0 : static_cast<size_t>(present);
  if (facts.teams)
    for (const gn::ReportTeam& team : out.teams)
      for (const TallySeat& s : t.seats)
        if (s.on && s.team == team.name) {
          sides++;
          break;
        }
  out.ended = gn::MatchEnd::broken;  // an error stopped the map, or it was changed by hand
  if (!facts.stopped && (frag_limit || time_limit))
    out.ended = gn::MatchEnd::limit;
  else if (!facts.stopped && sides < sides_needed)
    out.ended = gn::MatchEnd::forfeit;
  out.players = std::move(players);
  return true;
}

}  // namespace shooters_edge::net
