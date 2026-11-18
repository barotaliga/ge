// The lobby's match rules on the classic server (classic_match_rules.hpp states them).
//
// ge:summary The classic server's overtime frame, prewar's buttons and the lock's refusal
// ge:keywords overtime_frame tally_score sv_overtime timelimit prewar_buttons lock_refusal
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_game_test.cpp
#include "shooters_edge/net/classic_match_rules.hpp"

#include <algorithm>

#include "core/num_text.hpp"
#include "shooter/pmove.hpp"

namespace shooters_edge::net {

namespace gs = ge::server;
namespace sv = shooters_edge::server;

namespace {

double rule(const sv::RulesRunner& rules, const char* name) {
  const ge::console::Cvar* v = rules ? rules->cvars.find(name) : nullptr;
  return v ? v->number : 0;
}

}  // namespace

gs::MatchScore tally_score(const MatchTally& t, bool teams) {
  std::vector<gs::ScoredPlayer> players;
  for (const TallySeat& s : t.seats)
    if (s.on) players.push_back({s.team, s.frags - s.base, true});
  for (const TallySeat& s : t.left) players.push_back({s.team, s.frags - s.base, false});
  return gs::match_score(players, teams);
}

void overtime_frame(OvertimeRun& run, const sv::RulesRunner& rules, const MatchTally& t,
                    ClassicServer* to, bool in_match) {
  int64_t deaths = 0;
  for (const TallySeat& s : t.seats) deaths += s.on ? s.deaths : 0;
  for (const TallySeat& s : t.left) deaths += s.deaths;
  if (!in_match || !t.counting) {
    gs::overtime_reset(run.overtime);
    run.deaths = deaths;
    run.over = false;
    return;
  }
  gs::OvertimeSettings s;
  s.mode = gs::overtime_mode(static_cast<int>(rule(rules, "sv_overtime")));
  s.minutes = static_cast<int>(rule(rules, "sv_overtime_minutes"));
  s.short_gap = rule(rules, "deathmatch") == 4;
  const gs::MatchScore score = tally_score(t, rule(rules, "teamplay") != 0);
  gs::Overtime& o = run.overtime;
  const double played = rules->sv.time, frame_s = rules->sv.set.mintic;
  bool ends = false;
  if (deaths != run.deaths) {
    run.deaths = deaths;
    ends = gs::overtime_died(o, s, score);
  }
  const double limit = rule(rules, "timelimit");
  const bool clock = o.playing == gs::OvertimeMode::off || o.playing == gs::OvertimeMode::extra;
  if (!ends && !run.over && clock && limit > 0 && played + frame_s >= limit * 60) {
    switch (gs::overtime_time_up(o, s, score)) {
      case gs::TimeUp::end:
        break;  // the mod ends it on its own clock
      case gs::TimeUp::extend:
        rules.console("timelimit " +
                      ge::core::num::to_string(limit + std::clamp(s.minutes, 1, 999)));
        break;
      case gs::TimeUp::no_clock:
        rules.console("timelimit 0");
        break;
    }
  }
  if (ends && !run.over) {
    run.over = true;
    rules.console("timelimit " + ge::core::num::to_string(played / 60));  // run out next frame
  }
  if (to != nullptr)
    for (const std::string& line : o.said) classic_print_all(*to, line + "\n");
  o.said.clear();
}

void lobby_rules(gs::LobbySettings& l, const gs::ServerConfig& c, bool teams) {
  l.prewar = c.prewar;
  l.idle_kick_s = c.idle_kick_s;
  l.team_size_min = c.team_size_min;
  l.lock_join = c.lock_join;
  l.teams = teams;
}

uint8_t prewar_buttons(const gs::Lobby& l, int slot, uint8_t buttons, double now, double& held_told,
                       std::string& tell) {
  uint8_t held = 0;
  if (!gs::lobby_may_fire(l, slot)) held |= ge::shooter::kButtonAttack;
  if (!gs::lobby_may_jump(l, slot)) held |= ge::shooter::kButtonJump;
  if ((buttons & held) != 0 && l.phase == gs::LobbyPhase::warmup &&
      (held_told == 0 || now - held_told >= 1)) {
    held_told = now;
    tell = l.settings.prewar == 2 ? "Say ready to fire and jump.\n"
                                  : "Nobody may fire before the match.\n";
  }
  return static_cast<uint8_t>(buttons & ~held);
}

std::string lock_refusal(const gs::Lobby& l, const std::string& name) {
  if (gs::lobby_may_play(l, name) == gs::LobbyWhy::ok) return {};
  return "The match is under way and closed to new players, so you join as a spectator.\n";
}

}  // namespace shooters_edge::net
