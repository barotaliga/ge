// The arena's world: worldspawn, the frame's start (the settings, the time and frag limits, the
// clock in the loaded game's words, the end of the match), the spawn points and what a teleporter
// does to whoever it sends.
//
// ge:summary The arena's worldspawn, the frame's start, the clock and the match's end in C++
// ge:keywords arena native-arena worldspawn startframe end_match clock_frame fraglimit
// ge:keywords timelimit team-scores teleport_takes
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/arena_decl.hpp"

namespace shooters_edge::server::arena {

namespace {

// The teams' frags in a game of exactly two named teams: whether there are two (`team_a` and
// `team_b` then named, `frags_a` and `frags_b` theirs).
bool two_teams(A& q) {
  q.set_ge(g_team_a, 0);
  q.set_ge(g_team_b, 0);
  q.set_g(g_frags_a, 0);
  q.set_g(g_frags_b, 0);
  int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player));
  while (p != 0) {
    const int32_t t = q.infokey(p, q.s(s_team));
    if (!q.is(t, "")) {
      const bool a_free = q.is(q.ge(g_team_a), "");
      const bool a_same = q.same(t, q.ge(g_team_a));
      const bool b_free = q.is(q.ge(g_team_b), "");
      const bool b_same = q.same(t, q.ge(g_team_b));
      if (a_free | a_same) {
        q.set_ge(g_team_a, t);
        q.set_g(g_frags_a, q.g(g_frags_a) + q.f(p, q.L.f.frags));
      } else if (b_free | b_same) {
        q.set_ge(g_team_b, t);
        q.set_g(g_frags_b, q.g(g_frags_b) + q.f(p, q.L.f.frags));
      } else {
        return false;
      }
    }
    p = q.find(p, q.L.f.classname, q.s(s_player));
  }
  return !q.is(q.ge(g_team_b), "");
}

float positive(float f) { return f > 0 ? f : 0; }

// A share as text with one decimal ("56.0").
int32_t one_decimal(A& q, float share) {
  const float tenths = q.rint(share * 10);
  const float whole = q.floor(tenths / 10);
  const int32_t w = q.ftos(whole);
  return q.strcat({w, q.s(s_point), q.ftos(tenths - whole * 10)});
}

void say_line(A& q, int32_t text) { say_all(q, kPrintHigh, {text}); }

// The teams' scores in the loaded game's words: each team's frags and its share of the frags.
void say_team_scores(A& q) {
  const bool off = q.g(g_teamplay) == 0;
  const bool two = two_teams(q);  // asked either way: no short circuit
  if (off | !two) return;
  float sum = 0;
  if (q.g(g_frags_a) > 0) sum = sum + q.g(g_frags_a);
  if (q.g(g_frags_b) > 0) sum = sum + q.g(g_frags_b);
  if (sum <= 0) sum = 1;
  say_line(q, game_words(q, q.s(s_team_scores_head), 0, 0, 0, 0));
  for (Gl team : {g_team_a, g_team_b}) {
    const Gl frags = team == g_team_a ? g_frags_a : g_frags_b;
    const int32_t name = q.ge(team);
    const int32_t count = q.ftos(q.g(frags));
    const int32_t share = one_decimal(q, 100 * positive(q.g(frags)) / sum);
    say_line(q, game_words(q, q.s(s_team_scores_row), name, count, share, 0));
  }
  say_line(q, game_words(q, q.s(s_team_scores_tail), 0, 0, 0, 0));
}

// The clock in the loaded game's words (none in ours): each whole minute left, then 30, 15 and
// the last ten seconds, and in a two-team game who leads at each minute.
void clock_frame(A& q) {
  {
    const bool off = q.g(g_timelimit) == 0;
    const bool silent = q.is(game_words(q, q.s(s_clock_minutes), 0, 0, 0, 0), "");
    if (off | silent) return;
  }
  const float left = q.ceil(q.g(g_timelimit) * 60 - q.time());
  const float prev = q.g(g_clock_said);
  if (left == prev) return;
  q.set_g(g_clock_said, left);
  if ((prev == 0) | (left <= 0)) return;
  if (left >= 60) {
    const float m = q.floor(left / 60);
    if (left - m * 60 != 0) return;
    if (m == 1) {
      say_line(q, game_words(q, q.s(s_clock_minute), q.ftos(m), 0, 0, 0));
    } else {
      say_line(q, game_words(q, q.s(s_clock_minutes), q.ftos(m), 0, 0, 0));
    }
    const bool on_teams = q.g(g_teamplay) != 0;
    const bool two = two_teams(q);  // asked either way: no short circuit
    if (on_teams & two) {
      const float a = q.g(g_frags_a);
      const float b = q.g(g_frags_b);
      if (a == b) {
        say_line(q, game_words(q, q.s(s_clock_tie), 0, 0, 0, 0));
      } else if ((a > b) & (a - b == 1)) {
        say_line(q, game_words(q, q.s(s_clock_lead_one), q.ge(g_team_a), q.s(s_one), 0, 0));
      } else if (a > b) {
        say_line(q, game_words(q, q.s(s_clock_lead), q.ge(g_team_a), q.ftos(a - b), 0, 0));
      } else if (b - a == 1) {
        say_line(q, game_words(q, q.s(s_clock_lead_one), q.ge(g_team_b), q.s(s_one), 0, 0));
      } else {
        say_line(q, game_words(q, q.s(s_clock_lead), q.ge(g_team_b), q.ftos(b - a), 0, 0));
      }
    }
    return;
  }
  if ((left == 30) | (left == 15) | (left <= 10)) {
    if (left == 1) {
      say_line(q, game_words(q, q.s(s_clock_second), q.s(s_one), 0, 0, 0));
    } else {
      say_line(q, game_words(q, q.s(s_clock_seconds), q.ftos(left), 0, 0, 0));
    }
  }
}

// Ends the match: everyone held where they stand, the scores printed, the map again in 5 s.
void end_match(A& q, int32_t why) {
  q.set_g(g_match_over, 1);
  q.set_g(g_intermission_time, q.time());
  const int32_t over = game_words(q, q.s(s_match_over), 0, 0, 0, 0);
  if (!q.is(over, "")) {
    say_all(q, kPrintHigh, {over});
  } else {
    say_all(q, kPrintHigh, {why, q.s(s_nl)});
  }
  int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player));
  while (p != 0) {
    q.set_f(p, q.L.f.takedamage, kDamageNo);
    q.set_f(p, q.L.f.movetype, kMovetypeNone);
    q.set_v(p, q.L.f.velocity, {0, 0, 0});
    q.set_f(p, q.L.f.button0, 0);
    if (q.is(over, "")) {
      say_all(q, kPrintHigh,
              {q.i(p, q.L.f.netname), q.s(s_colon), q.ftos(q.f(p, q.L.f.frags)), q.s(s_frags_nl)});
    }
    p = q.find(p, q.L.f.classname, q.s(s_player));
  }
  if (!q.is(over, "")) say_team_scores(q);
}

}  // namespace

// void() worldspawn: the match afresh, everything the rules name precached, the light styles.
void worldspawn(A& q) {
  q.set_ge(g_last_spawn, q.world());
  q.set_g(g_match_over, 0);
  q.set_g(g_intermission_time, 0);
  // Read now too: the map's items spawn before the first frame, and deathmatch 4 keeps fewer.
  q.set_g(g_deathmatch, q.cvar(q.s(s_deathmatch)));
  player_precache(q);
  weapon_precache(q);
  item_precache(q);
  dmm4_precache(q);
  q.precache_model(q.s(s_backpack_mdl));
  for (St s : {s_r_tele1_wav, s_drclos4_wav, s_doormv1_wav, s_plat1_wav, s_plat2_wav,
               s_switch21_wav, s_lock4_wav, s_pkup_wav, s_armor1_wav})
    q.precache_sound(q.s(s));
  q.set_g(g_clock_said, 0);
  // A loaded game's words bring their sounds: the powers wearing off and the lobby's countdown.
  if (!q.is(game_words(q, q.s(s_power_quad_off), 0, 0, 0, 0), "")) {
    q.precache_sound(q.s(s_damage2_wav));
    q.precache_sound(q.s(s_protect2_wav));
    q.precache_sound(q.s(s_switch04_wav));
  }
  // Light style 0 is the steady light; 1.. flicker and pulse.
  q.lightstyle(0, q.s(s_style0));
  q.lightstyle(1, q.s(s_style1));
  q.lightstyle(2, q.s(s_style2));
}

// void() StartFrame: the settings read, the clock, the limits, the map again after the match.
void StartFrame(A& q) {
  q.set_g(g_deathmatch, q.cvar(q.s(s_deathmatch)));
  q.set_g(g_teamplay, q.cvar(q.s(s_teamplay)));
  q.set_g(g_fraglimit, q.cvar(q.s(s_fraglimit)));
  q.set_g(g_timelimit, q.cvar(q.s(s_timelimit)));
  if (on(q.g(g_match_over))) {
    if (q.time() > q.g(g_intermission_time) + 5) {
      q.changelevel(q.mapname());
      q.set_g(g_intermission_time, q.time() + 1000);  // asked once
    }
    return;
  }
  clock_frame(q);
  if ((q.g(g_timelimit) != 0) & (q.time() >= q.g(g_timelimit) * 60)) {
    end_match(q, q.s(s_timelimit_hit));
    return;
  }
  if (on(q.g(g_fraglimit))) {
    for (int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player)); p != 0;
         p = q.find(p, q.L.f.classname, q.s(s_player))) {
      if (q.f(p, q.L.f.frags) >= q.g(g_fraglimit)) {
        end_match(q, q.s(s_fraglimit_hit));
        return;
      }
    }
  }
}

void main_hook(A&) {}
void info_player_deathmatch(A&) {}
void info_player_start(A&) {}
void info_intermission(A&) {}

// A teleporter sends a living player, and arriving takes the spot: whoever stands there dies.
bool teleport_takes(A& q, int32_t who) {
  return q.is(q.i(who, q.L.f.classname), "player") & (q.f(who, q.L.f.health) > 0);
}
void teleport_arrived(A& q, int32_t who) { telefrag(q, who); }

}  // namespace shooters_edge::server::arena
