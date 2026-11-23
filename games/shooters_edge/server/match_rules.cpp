// The match every classic server plays (match_rules.hpp states the rules).
//
// ge:summary The shared match rules: overtime's step, prewar's buttons, the lock, warmup's hold
// ge:keywords overtime_step prewar_buttons lock_refusal warmup_hold timeout_hold phase_status
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_match_test.cpp
#include "shooters_edge/server/match_rules.hpp"

#include <algorithm>

#include "core/num_text.hpp"
#include "shooter/pmove.hpp"
#include "shooters_edge/server/commands.hpp"

namespace shooters_edge::server {

namespace {

double rule(const RulesRunner& rules, const char* name) {
  const ge::console::Cvar* v = rules ? rules->cvars.find(name) : nullptr;
  return v ? v->number : 0;
}

std::string rule_text(const RulesRunner& rules, const char* name) {
  const ge::console::Cvar* v = rules ? rules->cvars.find(name) : nullptr;
  return v ? v->value : std::string("0");
}

}  // namespace

void overtime_step(OvertimeRun& run, const RulesRunner& rules, const gsv::MatchScore& score,
                   int64_t deaths, bool in_match, std::vector<std::string>& said) {
  if (!in_match) {
    gsv::overtime_reset(run.overtime);
    run.deaths = deaths;
    run.over = false;
    return;
  }
  gsv::OvertimeSettings s;
  s.mode = gsv::overtime_mode(static_cast<int>(rule(rules, "sv_overtime")));
  s.minutes = static_cast<int>(rule(rules, "sv_overtime_minutes"));
  s.short_gap = rule(rules, "deathmatch") == 4;
  gsv::Overtime& o = run.overtime;
  const double played = rules->sv.time, frame_s = rules->sv.set.mintic;
  bool ends = false;
  if (deaths != run.deaths) {
    run.deaths = deaths;
    ends = gsv::overtime_died(o, s, score);
  }
  const double limit = rule(rules, "timelimit");
  const bool clock = o.playing == gsv::OvertimeMode::off || o.playing == gsv::OvertimeMode::extra;
  if (!ends && !run.over && clock && limit > 0 && played + frame_s >= limit * 60) {
    switch (gsv::overtime_time_up(o, s, score)) {
      case gsv::TimeUp::end:
        break;  // the rules end it on their own clock
      case gsv::TimeUp::extend:
        rules.console("timelimit " +
                      ge::core::num::to_string(limit + std::clamp(s.minutes, 1, 999)));
        break;
      case gsv::TimeUp::no_clock:
        rules.console("timelimit 0");
        break;
    }
  }
  if (ends && !run.over) {
    run.over = true;
    rules.console("timelimit " + ge::core::num::to_string(played / 60));  // run out next frame
  }
  for (std::string& line : o.said) said.push_back(std::move(line));
  o.said.clear();
}

void lobby_rules(gsv::LobbySettings& l, const gsv::ServerConfig& c, bool teams) {
  l.prewar = c.prewar;
  l.idle_kick_s = c.idle_kick_s;
  l.team_size_min = c.team_size_min;
  l.lock_join = c.lock_join;
  l.teams = teams;
}

uint8_t prewar_buttons(const gsv::Lobby& l, int slot, uint8_t buttons, double now,
                       double& held_told, std::string& tell, bool jump) {
  uint8_t held = 0;
  if (!gsv::lobby_may_fire(l, slot)) held |= ge::shooter::kButtonAttack;
  if (jump && !gsv::lobby_may_jump(l, slot)) held |= ge::shooter::kButtonJump;
  const bool told_jump = !jump && prewar_jump_held(l, slot);
  if (((buttons & held) != 0 || told_jump) && l.phase == gsv::LobbyPhase::warmup &&
      (held_told == 0 || now - held_told >= 1)) {
    held_told = now;
    tell = l.settings.prewar == 2 ? "Say ready to fire and jump.\n"
                                  : "Nobody may fire before the match.\n";
  }
  return static_cast<uint8_t>(buttons & ~held);
}

bool prewar_jump_held(const gsv::Lobby& l, int slot) { return !gsv::lobby_may_jump(l, slot); }

std::string lock_refusal(const gsv::Lobby& l, const std::string& name) {
  if (gsv::lobby_may_play(l, name) == gsv::LobbyWhy::ok) return {};
  return "The match is under way and closed to new players, so you join as a spectator.\n";
}

void warmup_hold(WarmupHold& h, const RulesRunner& rules, bool match) {
  if (!rules) return;
  if (match) {
    if (!h.held) return;
    rules.console("fraglimit " + h.fraglimit);
    rules.console("timelimit " + h.timelimit);
    h.held = false;
    return;
  }
  const std::string frags = rule_text(rules, "fraglimit"), time = rule_text(rules, "timelimit");
  if (!h.held) {
    h.fraglimit = frags;
    h.timelimit = time;
    h.held = true;
  } else {  // a value set meanwhile is the one the match gets
    if (frags != "0") h.fraglimit = frags;
    if (time != "0") h.timelimit = time;
  }
  if (frags != "0") rules.console("fraglimit 0");
  if (time != "0") rules.console("timelimit 0");
}

void timeout_hold(const gsv::TimeoutBook& b, RulesRunner& rules, bool& held) {
  const bool want = gsv::timeout_paused(b);
  if (want == held || !rules) return;
  rules.pause(kPauseByServer, want);
  held = want;
}

const char* phase_status(gsv::LobbyPhase p) {
  return p == gsv::LobbyPhase::warmup      ? "Standby"
         : p == gsv::LobbyPhase::countdown ? "Countdown"
         : p == gsv::LobbyPhase::match     ? "In progress"
                                           : "End";
}

}  // namespace shooters_edge::server
