// The campaign's world: worldspawn, the frame's start (the intermission and the way to the next
// map), the spawn points, and the campaign's own map things: the exits and the intermission,
// secrets, counters and relays, the skill triggers, the registered-only triggers, the episode and
// boss gates, the runes and the keys, and the words at each episode's end.
//
// ge:summary The campaign's world in C++: worldspawn, the intermission, exits, secrets, runes, keys
// ge:keywords campaign native-campaign worldspawn intermission changelevel trigger_secret
// ge:keywords trigger_counter sigil episode-gate keys finale_words
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/campaign_decl.hpp"

namespace shooters_edge::server::rules::campaign {

namespace {

// During the intermission: once its wait is over, any player's button leaves for the next map,
// or first shows the episode's closing words when it has them (then a second button leaves).
void intermission_frame(A& q) {
  if (q.time() < q.g(g_intermission_exit)) return;
  int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player));
  while (p != 0) {
    if ((q.f(p, q.L.f.button0) != 0) | (q.f(p, q.L.f.button1) != 0) |
        (q.f(p, q.L.f.button2) != 0)) {
      if (!q.is(q.ge(g_finale_words), "")) {
        q.WriteByte(kMsgAll, kSvcFinale);
        q.WriteString(kMsgAll, q.ge(g_finale_words));
        q.set_ge(g_finale_words, q.s(s_empty));
        q.set_g(g_intermission_exit, q.time() + 2);
        return;
      }
      q.set_g(g_intermission_exit, q.time() + 1000);  // asked once
      q.changelevel(q.ge(g_next_map));
      return;
    }
    p = q.find(p, q.L.f.classname, q.s(s_player));
  }
}

// Where the players watch the intermission from: a random intermission spot, else the start.
int32_t intermission_spot(A& q) {
  int32_t spot = q.find(q.world(), q.L.f.classname, q.s(s_info_intermission));
  float n = 0;
  int32_t pick = q.world();
  while (spot != 0) {
    n = n + 1;
    if (q.random() * n < 1) pick = spot;
    spot = q.find(spot, q.L.f.classname, q.s(s_info_intermission));
  }
  if (pick == 0) pick = q.find(q.world(), q.L.f.classname, q.s(s_info_player_start));
  return pick;
}

// The episode (its bit, as a rune's) a start map's way in leads to; 0 for any other map.
float episode_of(A& q, int32_t m) {
  if (q.is(m, "e1m1")) return 1;
  if (q.is(m, "e2m1")) return 2;
  if (q.is(m, "e3m1")) return 4;
  if (q.is(m, "e4m1")) return 8;
  return 0;
}

// The episodes open one at a time, alone or in coop: the first always, each next once the one
// before is done (`sv_episodes_done`, a rune's bit for each, kept by the host from game to game,
// which also keeps the bars in front of a shut episode down).
bool episode_shut(A& q, int32_t m) {
  if ((q.deathmatch() != 0) | !q.is(q.mapname(), "start")) return false;
  const float bit = episode_of(q, m);
  if (bit <= 1) return false;
  return band(q.cvar(q.s(s_sv_episodes_done)), bit / 2) == 0;
}

// A pick-up the size of a player, its model off the map's entity.
void pickup_start(A& q, int32_t mdl) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.solid, kSolidTrigger);
  q.set_f(me, q.L.f.movetype, kMovetypeToss);
  q.setmodel(me, mdl);
  q.setsize(me, {-16, -16, -24}, {16, 16, 32});
  q.setorigin(me, q.v(me, q.L.f.origin) + Vec3{0, 0, 1});
}

// Each episode's closing words, shown when its last map is left (our own words).
int32_t episode_words(A& q, float bit) {
  if (on(band(bit, 1))) return q.s(s_the_first_rune_hums);
  if (on(band(bit, 2))) return q.s(s_the_second_rune_is);
  if (on(band(bit, 4))) return q.s(s_three_runes_now_the);
  return q.s(s_the_last_rune_is);
}

bool is_player(A& q, int32_t e) { return q.is(q.i(e, q.L.f.classname), "player"); }

}  // namespace

void worldspawn(A& q) {
  q.set_ge(g_last_spawn, q.world());
  q.set_g(g_match_over, 0);
  q.set_g(g_intermission_running, 0);
  q.set_ge(g_next_map, q.s(s_empty));
  q.set_ge(g_finale_words, q.s(s_empty));
  player_precache(q);
  weapon_precache(q);
  item_precache(q);
  ai_precache(q);
  for (St s : {s_r_tele1_wav, s_drclos4_wav, s_doormv1_wav, s_plat1_wav, s_plat2_wav,
               s_switch21_wav, s_lock4_wav, s_pkup_wav, s_armor1_wav, s_misc_secret_wav,
               s_misc_talk_wav})
    q.precache_sound(q.s(s));
  q.lightstyle(0, q.s(s_style0));
  q.lightstyle(1, q.s(s_style1));
  q.lightstyle(2, q.s(s_style2));
}

void StartFrame(A& q) {
  if (on(q.g(g_intermission_running))) intermission_frame(q);
}

void main_hook(A&) {}

// THE SPAWN POINTS: marks the rules find by class name.
void info_player_start(A&) {}
void info_player_start2(A&) {}
void info_player_coop(A&) {}
void info_player_deathmatch(A&) {}
void info_intermission(A&) {}
void info_null(A& q) { q.remove(q.self()); }
void info_notnull(A&) {}

// The level is over: every player held at the intermission spot, unseen and unhurt, and told.
void intermission_start(A& q) {
  q.set_g(g_intermission_running, 1);
  q.set_g(g_match_over, 1);
  if (on(q.deathmatch())) q.set_g(g_intermission_exit, q.time() + 5);
  else q.set_g(g_intermission_exit, q.time() + 2);
  const int32_t spot = intermission_spot(q);
  int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player));
  while (p != 0) {
    q.set_v(p, q.L.f.view_ofs, {0, 0, 0});
    q.set_v(p, q.L.f.angles, q.v(spot, f_mangle));
    q.set_v(p, q.L.f.v_angle, q.v(spot, f_mangle));
    q.set_f(p, q.L.f.fixangle, 1);
    q.set_f(p, q.L.f.takedamage, kDamageNo);
    q.set_f(p, q.L.f.solid, kSolidNot);
    q.set_f(p, q.L.f.movetype, kMovetypeNone);
    q.set_v(p, q.L.f.velocity, {0, 0, 0});
    q.set_f(p, q.L.f.modelindex, 0);
    q.set_i(p, q.L.f.weaponmodel, q.s(s_empty));
    q.setorigin(p, q.v(spot, q.L.f.origin));
    p = q.find(p, q.L.f.classname, q.s(s_player));
  }
  q.WriteByte(kMsgAll, kSvcIntermission);
}

void changelevel_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  if (!is_player(q, other) | (q.g(g_intermission_running) != 0)) return;
  if (episode_shut(q, q.i(me, f_map))) {
    if (q.time() >= q.f(me, f_attack_finished)) {
      q.centerprint(other, {q.s(s_this_episode_opens_once)});
      q.set_f(me, f_attack_finished, q.time() + 2);
    }
    return;
  }
  q.set_ge(g_next_map, q.i(me, f_map));
  q.set_ge(g_activator, other);
  use_targets(q);
  if ((band(q.f(q.self(), q.L.f.spawnflags), 1) != 0) & (q.deathmatch() == 0)) {
    q.changelevel(q.ge(g_next_map));  // no intermission: straight on
    return;
  }
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_noop));
  q.set_i(q.self(), q.L.f.think, q.fn(fn_intermission_start));
  q.set_f(q.self(), q.L.f.nextthink, q.time() + 0.1f);
}

void trigger_changelevel(A& q) {
  if (q.is(q.i(q.self(), f_map), "")) q.objerror({q.s(s_a_changelevel_trigger_without)});
  trigger_start(q);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_changelevel_touch));
}

// THE SECRETS, COUNTERS AND RELAYS.
void secret_touch(A& q) {
  const int32_t other = q.other();
  if (!is_player(q, other) | (q.f(other, q.L.f.health) <= 0)) return;
  q.add_to(q.L.g.found_secrets, 1);
  q.WriteByte(kMsgAll, kSvcFoundsecret);
  const int32_t me = q.self();
  if (q.is(q.i(me, q.L.f.message), "")) q.set_i(me, q.L.f.message, q.s(s_a_secret));
  q.sound(other, kChanItem, q.s(s_misc_secret_wav), 1, kAttnNorm);
  q.set_ge(g_activator, other);
  q.set_f(me, f_wait, -1);
  trigger_fire(q);
}

void trigger_secret(A& q) {
  q.add_to(q.L.g.total_secrets, 1);
  trigger_start(q);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_secret_touch));
}

// Fires its targets on its `count`th use (2 when not set); each use before it counts down out
// loud unless spawn flag 1 keeps it quiet.
void counter_use(A& q) {
  const int32_t me = q.self();
  q.set_f(me, f_count, q.f(me, f_count) - 1);
  if (q.f(me, f_count) < 0) return;
  const int32_t activator = q.ge(g_activator);
  if (q.f(me, f_count) > 0) {
    if (is_player(q, activator) & (band(q.f(me, q.L.f.spawnflags), 1) == 0)) {
      q.centerprint(activator, {q.ftos(q.f(me, f_count)), q.s(s_more_to_go)});
    }
    return;
  }
  if (is_player(q, activator) & (band(q.f(me, q.L.f.spawnflags), 1) == 0))
    q.centerprint(activator, {q.s(s_done)});
  use_targets(q);
}

void trigger_counter(A& q) {
  if (q.f(q.self(), f_count) == 0) q.set_f(q.self(), f_count, 2);
  q.set_i(q.self(), q.L.f.use, q.fn(fn_counter_use));
}

void trigger_relay(A& q) { q.set_i(q.self(), q.L.f.use, q.fn(fn_use_targets)); }

// THE SKILL AND THE FULL GAME.
void setskill_touch(A& q) {
  if (!is_player(q, q.other())) return;
  q.cvar_set(q.s(s_skill), q.i(q.self(), q.L.f.message));
}

void trigger_setskill(A& q) {
  trigger_start(q);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_setskill_touch));
}

// Acts as a trigger_once on the full game's packs; on the shareware ones it only shows its message.
void onlyregistered_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  if (!is_player(q, other) | (q.f(other, q.L.f.health) <= 0)) return;
  if (q.f(me, f_attack_finished) > q.time()) return;
  q.set_f(me, f_attack_finished, q.time() + 2);
  if (on(q.cvar(q.s(s_registered)))) {
    q.set_i(me, q.L.f.message, q.s(s_empty));
    q.set_ge(g_activator, other);
    q.set_f(me, f_wait, -1);
    trigger_fire(q);
    return;
  }
  if (!q.is(q.i(me, q.L.f.message), "")) {
    q.centerprint(other, {q.i(me, q.L.f.message)});
    q.sound(other, kChanBody, q.s(s_misc_talk_wav), 1, kAttnNorm);
  }
}

void trigger_onlyregistered(A& q) {
  trigger_start(q);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_onlyregistered_touch));
}

void trigger_monsterjump(A& q) { trigger_start(q); }

// THE EPISODES.
// Stands (blocking the hall) once its episode is done.
void func_episodegate(A& q) {
  if (band(q.serverflags(), q.f(q.self(), q.L.f.spawnflags)) == 0) {
    q.remove(q.self());
    return;
  }
  func_wall(q);
}

// Gone once all four episodes are done.
void func_bossgate(A& q) {
  if (band(q.serverflags(), 15) == 15) {
    q.remove(q.self());
    return;
  }
  func_wall(q);
}

// The rune: its episode's bit in serverflags, and the episode's closing words for the way out.
void sigil_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  if (!is_player(q, other) | (q.f(other, q.L.f.health) <= 0)) return;
  const float bits = band(q.f(me, q.L.f.spawnflags), 15);
  q.set_gf(q.L.g.serverflags, bor(q.serverflags(), bits));
  // In coop the next episode opens for good (the host keeps it from game to game).
  if (on(q.coop())) {
    const float done = bor(q.cvar(q.s(s_sv_episodes_done)), band(q.f(me, q.L.f.spawnflags), 15));
    q.cvar_set(q.s(s_sv_episodes_done), q.ftos(done));
  }
  if (q.deathmatch() == 0)
    q.set_ge(g_finale_words, episode_words(q, band(q.f(me, q.L.f.spawnflags), 15)));
  q.centerprint(other, {q.s(s_the_episode_s_rune_is_yours)});
  q.sound(other, kChanItem, q.s(s_pkup_wav), 1, kAttnNorm);
  q.set_ge(g_activator, other);
  use_targets(q);
  q.remove(q.self());
}

// Its model by its episode's bit (the first episode's when the flags name none).
void item_sigil(A& q) {
  const float flags = q.f(q.self(), q.L.f.spawnflags);
  int32_t mdl = q.s(s_end1_mdl);
  if (on(band(flags, 2))) mdl = q.s(s_end2_mdl);
  if (on(band(flags, 4))) mdl = q.s(s_end3_mdl);
  if (on(band(flags, 8))) mdl = q.s(s_end4_mdl);
  q.precache_model(mdl);
  pickup_start(q, mdl);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_sigil_touch));
}

// A key: taken by the first player in single player; in coop every player may take it.
void key_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  if (!is_player(q, other) | (q.f(other, q.L.f.health) <= 0)) return;
  if (on(band(q.f(other, q.L.f.items), q.f(me, q.L.f.items)))) return;
  q.set_f(other, q.L.f.items, bor(q.f(other, q.L.f.items), q.f(me, q.L.f.items)));
  q.centerprint(other, {q.s(s_you_have_a_key)});
  q.sound(other, kChanItem, q.s(s_pkup_wav), 1, kAttnNorm);
  q.set_ge(g_activator, other);
  use_targets(q);
  if (q.coop() == 0) q.remove(q.self());
}

void item_key1(A& q) {
  q.precache_model(q.s(s_w_s_key_mdl));
  q.set_f(q.self(), q.L.f.items, static_cast<float>(kItKey1));
  pickup_start(q, q.s(s_w_s_key_mdl));
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_key_touch));
}

void item_key2(A& q) {
  q.precache_model(q.s(s_w_g_key_mdl));
  q.set_f(q.self(), q.L.f.items, static_cast<float>(kItKey2));
  pickup_start(q, q.s(s_w_g_key_mdl));
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_key_touch));
}

}  // namespace shooters_edge::server::rules::campaign
