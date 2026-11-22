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

namespace shooters_edge::server::rules::arena {

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

// QUAKE'S MAP CHANGE (`quake_rules`, porting/shooters-edge/qw-dm-rules.md § 20, 22 and round 2's
// § 15): the next map by `samelevel`, the key named after the map, the first level exit, else
// the same; the intermission's music and camera; held until a button after 5 s.
int32_t quake_next_map(A& q) {
  if (q.cvar(q.s(s_samelevel)) == 1) return q.mapname();
  const int32_t keyed = q.infokey(q.world(), q.mapname());
  if (!q.is(keyed, "")) return keyed;
  if (!q.is(q.ge(g_next_map), "")) return q.ge(g_next_map);
  return q.mapname();
}

// A random intermission spot, else the start (its angles sent as zeros).
int32_t quake_intermission_spot(A& q) {
  int32_t spot = q.find(q.world(), q.L.f.classname, q.s(s_info_intermission));
  float n = 0;
  int32_t pick = 0;
  while (spot != 0) {
    n = n + 1;
    if (q.random() * n < 1) pick = spot;
    spot = q.find(spot, q.L.f.classname, q.s(s_info_intermission));
  }
  if (pick == 0) pick = q.find(q.world(), q.L.f.classname, q.s(s_info_player_start));
  return pick;
}

void quake_intermission(A& q, int32_t next) {
  q.set_ge(g_next_map, q.strzone(next));
  q.set_g(g_intermission_exit, q.time() + 5);
  const int32_t spot = quake_intermission_spot(q);
  q.WriteByte(kMsgAll, kSvcCdtrack);
  q.WriteByte(kMsgAll, 3);
  q.WriteByte(kMsgAll, kSvcIntermission);
  const Vec3 at = q.v(spot, q.L.f.origin);
  const Vec3 look = q.v(spot, f_mangle);
  q.WriteCoord(kMsgAll, at.x);
  q.WriteCoord(kMsgAll, at.y);
  q.WriteCoord(kMsgAll, at.z);
  q.WriteAngle(kMsgAll, look.x);
  q.WriteAngle(kMsgAll, look.y);
  q.WriteAngle(kMsgAll, look.z);
  for (int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player)); p != 0;
       p = q.find(p, q.L.f.classname, q.s(s_player))) {
    q.set_f(p, q.L.f.takedamage, kDamageNo);
    q.set_f(p, q.L.f.solid, kSolidNot);
    q.set_f(p, q.L.f.movetype, kMovetypeNone);
    q.set_v(p, q.L.f.velocity, {0, 0, 0});
    q.set_f(p, q.L.f.modelindex, 0);
  }
}

// After its 5 s, the first player to press fire, jump or use takes everyone on; with nobody on
// the server, it goes on alone.
void quake_intermission_frame(A& q) {
  if (q.time() < q.g(g_intermission_exit)) return;
  bool anyone = false;
  bool pressed = false;
  for (int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player)); p != 0;
       p = q.find(p, q.L.f.classname, q.s(s_player))) {
    anyone = true;
    pressed = pressed | (q.f(p, q.L.f.button0) != 0) | (q.f(p, q.L.f.button1) != 0) |
              (q.f(p, q.L.f.button2) != 0);
  }
  // An empty server has nobody to press: it goes on by itself, so it never stalls.
  if (pressed | !anyone) {
    q.set_g(g_intermission_exit, q.time() + 1000);  // asked once
    q.changelevel(q.ge(g_next_map));
  }
}

// Ends the match: everyone held where they stand, the scores printed, the map again in 5 s.
void end_match(A& q, int32_t why) {
  q.set_g(g_match_over, 1);
  q.set_g(g_intermission_time, q.time());
  if (quake_rules(q)) {
    quake_intermission(q, quake_next_map(q));
    return;
  }
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
  q.set_ge(g_next_map, q.s(s_empty));
  // Read now too: the map's items spawn before the first frame, and deathmatch 4 keeps fewer.
  q.set_g(g_deathmatch, q.cvar(q.s(s_deathmatch)));
  midair_read(q);
  quake_read(q);
  if (quake_rules(q)) {
    quake_corpses_make(q);
    q.cvar_set(q.s(s_sv_gravity), q.s(q.is(q.mapname(), "e1m8") ? s_g100 : s_g800));
  }
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
  midair_read(q);
  quake_read(q);
  if (on(q.g(g_match_over))) {
    if (quake_rules(q)) {
      quake_intermission_frame(q);
      return;
    }
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
// A moving platform's corner (common_map.cpp `func_train`): kept, found by its name.
void path_corner(A&) {}
void info_player_start(A&) {}
void info_intermission(A&) {}

// A level exit: under Quake's rules the first one names the next map, and touching any in a
// match says so and ends the map to its own; `samelevel` 2 (3: but on the map `start`) kills
// whoever tries instead. The arena's own rules have no exits.
void trigger_changelevel(A& q) {
  const int32_t me = q.self();
  if (!quake_rules(q)) {
    q.remove(me);
    return;
  }
  if (q.is(q.ge(g_next_map), "")) q.set_ge(g_next_map, q.i(me, f_map));
  q.set_f(me, q.L.f.solid, kSolidTrigger);
  q.set_f(me, q.L.f.movetype, kMovetypeNone);
  q.setmodel(me, q.i(me, q.L.f.model));
  q.set_f(me, q.L.f.modelindex, 0);
  q.set_i(me, q.L.f.touch, q.fn(fn_changelevel_touch));
}

// The keys: Quake's under its rules; the arena's own rules have none.
void item_key1(A& q) {
  if (quake_rules(q)) {
    quake_key_start(q, kItKey1);
  } else {
    q.remove(q.self());
  }
}
void item_key2(A& q) {
  if (quake_rules(q)) {
    quake_key_start(q, kItKey2);
  } else {
    q.remove(q.self());
  }
}

// A relay: fires its targets when used (with its delay, words and kill target).
void trigger_relay(A& q) { q.set_i(q.self(), q.L.f.use, q.fn(fn_use_targets)); }

// A secret (Quake's rules): a trigger_once for players, its chime by `sounds` (1 when unset), its
// words "You found a secret area!" when it has none, the clients' secret counter told.
void trigger_secret(A& q) {
  const int32_t me = q.self();
  if (!quake_rules(q)) {
    q.remove(me);
    return;
  }
  if (q.f(me, q.L.f.sounds) == 0) q.set_f(me, q.L.f.sounds, 1);
  if (q.is(q.i(me, q.L.f.message), "")) q.set_i(me, q.L.f.message, q.s(s_found_secret_area));
  q.set_f(me, f_wait, -1);
  trigger_start(q);
  q.set_i(me, q.L.f.touch, q.fn(fn_secret_touch));
}

void secret_touch(A& q) {
  const int32_t o = q.other();
  if (!q.is(q.i(o, q.L.f.classname), "player") | (q.f(o, q.L.f.health) <= 0)) return;
  q.WriteByte(kMsgAll, kSvcFoundsecret);
  q.set_ge(g_activator, o);
  trigger_fire(q);
}

// void() SpectatorConnect and SpectatorDisconnect: under Quake's rules everyone is told when a
// spectator comes and goes; the arena's own keep quiet.
void SpectatorConnect(A& q) {
  quake_read(q);
  if (!quake_rules(q)) return;
  say_all(q, kPrintMedium,
          {q.s(s_spectator_word), q.i(q.self(), q.L.f.netname), q.s(s_entered_the_game)});
}

void SpectatorDisconnect(A& q) {
  if (!quake_rules(q)) return;
  say_all(q, kPrintMedium,
          {q.s(s_spectator_word), q.i(q.self(), q.L.f.netname), q.s(s_left_the_game)});
}

// void() SpectatorThink: under Quake's rules impulse 1 takes the spectator to the next
// deathmatch spot in turn, facing as it faces.
void SpectatorThink(A& q) {
  const int32_t me = q.self();
  if (!quake_rules(q) || (q.f(me, q.L.f.impulse) != 1)) {
    q.set_f(me, q.L.f.impulse, 0);
    return;
  }
  q.set_f(me, q.L.f.impulse, 0);
  int32_t spot = q.find(q.i(me, q.L.f.goalentity), q.L.f.classname, q.s(s_info_player_deathmatch));
  if (spot == 0) spot = q.find(q.world(), q.L.f.classname, q.s(s_info_player_deathmatch));
  if (spot == 0) return;
  q.set_i(me, q.L.f.goalentity, spot);
  q.setorigin(me, q.v(spot, q.L.f.origin));
  q.set_v(me, q.L.f.angles, q.v(spot, q.L.f.angles));
  q.set_f(me, q.L.f.fixangle, 1);
}

// A counter (Quake's rules): fires on its `count`th use (2 when unset), counting down out loud to
// a player unless spawnflag 1 keeps it quiet.
void trigger_counter(A& q) {
  const int32_t me = q.self();
  if (!quake_rules(q)) {
    q.remove(me);
    return;
  }
  if (q.f(me, f_count) == 0) q.set_f(me, f_count, 2);
  q.set_f(me, f_wait, -1);
  q.set_i(me, q.L.f.use, q.fn(fn_counter_use));
}

void counter_use(A& q) {
  const int32_t me = q.self();
  q.set_f(me, f_count, q.f(me, f_count) - 1);
  const float left = q.f(me, f_count);
  if (left < 0) return;
  const int32_t a = q.ge(g_activator);
  const bool told =
      q.is(q.i(a, q.L.f.classname), "player") & (band(q.f(me, q.L.f.spawnflags), 1) == 0);
  if (left > 0) {
    if (told) {
      const St words = left >= 4   ? s_more_to_go_many
                       : left == 3 ? s_only_3_more
                       : left == 2 ? s_only_2_more
                                   : s_only_1_more;
      q.centerprint(a, {q.s(words)});
    }
    return;
  }
  if (told) q.centerprint(a, {q.s(s_sequence_completed)});
  trigger_fire(q);
}

void changelevel_touch(A& q) {
  const int32_t me = q.self();
  const int32_t o = q.other();
  if (!q.is(q.i(o, q.L.f.classname), "player") | on(q.g(g_match_over))) return;
  const float same = q.cvar(q.s(s_samelevel));
  if ((same == 2) | ((same == 3) & !q.is(q.mapname(), "start"))) {
    q.set_i(o, f_deathtype, q.s(s_tried_to_leave));
    damage(q, o, me, me, 50000);
    return;
  }
  say_all(q, kPrintHigh, {q.i(o, q.L.f.netname), q.s(s_exited_the_level)});
  q.set_g(g_match_over, 1);
  q.set_g(g_intermission_time, q.time());
  quake_intermission(q, q.i(me, f_map));
}

// A teleporter sends a living player, and arriving takes the spot: whoever stands there dies.
bool teleport_takes(A& q, int32_t who) {
  return q.is(q.i(who, q.L.f.classname), "player") & (q.f(who, q.L.f.health) > 0);
}
void teleport_arrived(A& q, int32_t who) { telefrag(q, who); }

}  // namespace shooters_edge::server::rules::arena
