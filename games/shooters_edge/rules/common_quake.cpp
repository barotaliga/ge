// QUAKE'S OWN RULES on the arena: what a deathmatch of Quake and QuakeWorld does that the arena's
// own rules leave out or do another way, on while the loaded game asks for them (the server's
// local info `rules q1`, written by the game's profile; `quake_rules`). Each rule here is called
// from the arena's own where it differs: the player's body through pain, firing and death, the
// sounds of a hurt and a death, gibs and the head, the last four corpses kept, the respawn on a
// press once the body lies still, a landing heard and a hard one hurting, the spawn spot drawn at
// random with its flash, the kill command, each deathmatch mode's respawn times, the splash that
// a wall stops, the weapon taken in hand by the player's own `w_switch` and `b_switch`, and the
// powers' flashes and sounds. The facts are porting/shooters-edge/qw-dm-rules.md's; the campaign
// never sets the switch, so these are the arena's alone.
//
// ge:summary Quake's own deathmatch rules on the arena: bodies, deaths, spawns, modes, splash
// ge:keywords quake-rules rules-q1 deathmatch player-frames pain-frames death-frames gibs
// ge:keywords bodyque corpses respawn-press landing land-sound spawn-flash kill-command
// ge:keywords respawn-times weapon-stay splash-sight w_switch b_switch quad-sound ring-sound
// ge:skill progs shooter
// ge:test tests/shooters_edge_quake_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/variant.hpp"

namespace shooters_edge::server::rules::SE_RULES_NS {

namespace {

// The player model's frames by their sequences (measured: the model's frame names in order).
constexpr float kAxPain = 29, kPain = 35, kAxDeath = 41, kNailAtt = 103, kLightAtt = 105,
                kRockAtt = 107, kShotAtt = 113, kAxAtt = 119;
// The five deaths without the axe: first frame and length.
constexpr float kDeathFirst[5] = {50, 61, 70, 85, 94};
constexpr float kDeathFrames[5] = {11, 9, 15, 9, 9};

// One of `n` samples, `first` the first, drawn as rint(1 + (n - 1) * random): the first and the
// last half as often as each between.
int32_t one_of(A& q, std::initializer_list<St> samples) {
  const float n = static_cast<float>(samples.size());
  const float k = q.rint(1 + (n - 1) * q.random());
  float i = 1;
  for (St s : samples) {
    if (i == k) return q.s(s);
    i = i + 1;
  }
  return q.s(*samples.begin());
}

bool any_button(A& q, int32_t e) {
  return (q.f(e, q.L.f.button0) != 0) | (q.f(e, q.L.f.button1) != 0) | (q.f(e, q.L.f.button2) != 0);
}

Vec3 scaled(float k, Vec3 v) { return {k * v.x, k * v.y, k * v.z}; }

// A piece's speed from the health the body died on: up to 100 each way across and 200 to 300 up,
// times 0.7 above -50, 2 down to -200, 10 below.
Vec3 piece_velocity(A& q, float dm) {
  const float c1 = crandom(q) * 100;
  const float c2 = crandom(q) * 100;
  const float up = 200 + q.random() * 100;
  const Vec3 v{c1, c2, up};
  if (dm > -50) return scaled(0.7f, v);
  if (dm > -200) return scaled(2, v);
  return scaled(10, v);
}

Vec3 spin(A& q) {
  const float a = q.random() * 600;
  const float b = q.random() * 600;
  const float c = q.random() * 600;
  return {a, b, c};
}

void throw_piece(A& q, int32_t mdl, float dm) {
  const int32_t g = q.spawn();
  q.set_i(g, q.L.f.classname, q.s(s_gib));
  q.set_f(g, q.L.f.movetype, kMovetypeBounce);
  q.set_f(g, q.L.f.solid, kSolidNot);
  q.set_v(g, q.L.f.velocity, piece_velocity(q, dm));
  q.set_v(g, q.L.f.avelocity, spin(q));
  q.setmodel(g, mdl);
  q.setsize(g, {0, 0, 0}, {0, 0, 0});
  q.setorigin(g, q.v(q.self(), q.L.f.origin));
  q.set_i(g, q.L.f.think, q.fn(fn_remove_self));
  const float t = q.time() + 10;
  q.set_f(g, q.L.f.nextthink, t + q.random() * 10);
}

// The body becomes its head: thrown as a piece is, 24 lower, lying where it lands.
void become_head(A& q, float dm) {
  const int32_t me = q.self();
  q.setmodel(me, q.s(s_h_player_mdl));
  q.set_f(me, q.L.f.frame, 0);
  q.set_f(me, q.L.f.movetype, kMovetypeBounce);
  q.set_v(me, q.L.f.view_ofs, {0, 0, 8});
  q.setsize(me, {-16, -16, 0}, {16, 16, 56});
  q.set_v(me, q.L.f.velocity, piece_velocity(q, dm));
  q.set_v(me, q.L.f.origin, q.v(me, q.L.f.origin) - Vec3{0, 0, 24});
  const float flags = q.f(me, q.L.f.flags);
  q.set_f(me, q.L.f.flags, flags - band(flags, kFlOnground));
  q.set_v(me, q.L.f.avelocity, {0, crandom(q) * 600, 0});
}

// Five bubbles from the drowned, one each tenth of a second while the body is under.
void bubble_from(A& q, int32_t e) {
  const int32_t b = q.spawn();
  q.setmodel(b, q.s(s_s_bubble_spr));
  q.setorigin(b, q.v(e, q.L.f.origin) + Vec3{0, 0, 24});
  q.set_f(b, q.L.f.movetype, kMovetypeNoclip);
  q.set_f(b, q.L.f.solid, kSolidNot);
  q.set_v(b, q.L.f.velocity, {0, 0, 15});
  q.set_i(b, q.L.f.think, q.fn(fn_remove_self));
  q.set_f(b, q.L.f.nextthink, q.time() + 2);
  q.set_i(b, q.L.f.classname, q.s(s_bubble));
  q.setsize(b, {-8, -8, -8}, {8, 8, 8});
}

}  // namespace

bool quake_rules(A& q) { return on(q.g(g_quake)); }

// The switch and the keys it reads, each frame: `rules q1` in the local info, and `rj`.
void quake_read(A& q) {
  q.set_g(g_quake, q.is(q.infokey(q.world(), q.s(s_rules)), "q1") ? 1.0f : 0.0f);
  if (on(q.g(g_quake))) q.set_g(g_rj, number_of(q, q.infokey(q.world(), q.s(s_rj_key))));
}

// An action's frames (pain, firing, dying) from `first` to `last`, a tenth of a second each,
// held on the last when `hold` (a death), else back to running or standing after it.
void quake_act(A& q, float first, float last, bool hold, bool fire) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.frame, first);
  q.set_f(me, f_act_last, last);
  q.set_f(me, f_act_hold, hold ? 1.0f : 0.0f);
  q.set_f(me, f_act_fire, fire ? 1.0f : 0.0f);
  q.set_f(me, f_body_next, q.time() + 0.1f);
}

// The action's next frame when due: true while one plays (the run and stand cycles wait).
bool quake_acting(A& q) {
  const int32_t me = q.self();
  if (q.f(me, f_act_last) == 0) return false;
  if (q.time() < q.f(me, f_body_next)) return true;
  q.set_f(me, f_body_next, q.time() + 0.1f);
  const float next = q.f(me, q.L.f.frame) + 1;
  if (next <= q.f(me, f_act_last)) {
    q.set_f(me, q.L.f.frame, next);
    return true;
  }
  if (on(q.f(me, f_act_hold))) return true;
  q.set_f(me, f_act_last, 0);
  q.set_f(me, f_act_fire, 0);
  q.set_f(me, f_body_first, -1);  // the cycle starts again at its first frame
  return false;
}

// A firing's frames: the axe one of four swings, a gun its own, the spikers and the arc gun
// their two in turn while the trigger is held.
void quake_fire_frames(A& q, float w) {
  const int32_t me = q.self();
  if (w == kWBlade) {
    const float swing = q.floor(q.random() * 4);
    const float first = kAxAtt + 6 * (swing > 3 ? 3 : swing);
    quake_act(q, first, first + 5, false, true);
  } else if ((w == kWScatter) | (w == kWDouble)) {
    quake_act(q, kShotAtt, kShotAtt + 5, false, true);
  } else if ((w == kWGrenades) | (w == kWRockets)) {
    quake_act(q, kRockAtt, kRockAtt + 5, false, true);
  } else {
    const float first = w == kWArc ? kLightAtt : kNailAtt;
    const float at = q.f(me, q.L.f.frame) == first ? first + 1 : first;
    quake_act(q, at, at, false, true);
  }
}

// A hurt heard and seen: in lava or slime a burn's cry at every hurt; under water a drowning
// cry and a bubble at every hurt; else, once the last pain is half a second old, the axe's
// thud when the blade did it or one of six cries. None while firing, cloaked or dying, and the
// pain's frames with it.
void quake_pain(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.health) < 0) return;
  if (on(q.f(me, f_act_fire)) | (q.f(me, f_invisible_finished) > q.time())) return;
  const float type = q.f(me, q.L.f.watertype);
  int32_t sample = 0;
  if ((type == kContentLava) | (type == kContentSlime)) {
    sample = q.s(q.random() > 0.5f ? s_lburn1_wav : s_lburn2_wav);
  } else if ((type == kContentWater) & (q.f(me, q.L.f.waterlevel) == 3)) {
    sample = q.s(q.random() > 0.5f ? s_drown1_wav : s_drown2_wav);
    bubble_from(q, me);
  } else {
    if (q.time() < q.f(me, f_pain_finished)) return;
    q.set_f(me, f_pain_finished, q.time() + 0.5f);
    if (q.is(q.i(me, f_deathtype), "cut down")) {
      sample = q.s(s_axhit1_wav);
    } else {
      sample =
          one_of(q, {s_pain1_wav, s_pain2_wav, s_pain3_wav, s_pain4_wav, s_pain5_wav, s_pain6_wav});
    }
  }
  q.sound(me, kChanVoice, sample, 1, kAttnNorm);
  const float first = q.f(me, q.L.f.weapon) == kWBlade ? kAxPain : kPain;
  quake_act(q, first, first + 5, false, false);
}

// A death heard and seen (the arena's own has put the body down): the health no lower than -99;
// a body gibbed below -40 thrown as three pieces and its head; else one of five deaths (the
// axe's own with the blade in hand) and its cry, a drowning one under water with five bubbles;
// tossed up to 300 higher when it was not already rising; lying still once its frames are done.
void quake_die(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.health) < -99) q.set_f(me, q.L.f.health, -99);
  const float health = q.f(me, q.L.f.health);
  const Vec3 vel = q.v(me, q.L.f.velocity);
  if (vel.z < 10) q.set_v(me, q.L.f.velocity, {vel.x, vel.y, vel.z + q.random() * 300});
  q.set_f(me, f_act_fire, 0);
  if (health < -40) {
    const bool tele = q.is(q.i(me, f_deathtype), "telefragged");
    const int32_t cry =
        tele ? q.s(s_teledth1_wav) : q.s(q.random() < 0.5f ? s_gib_wav : s_udeath_wav);
    q.sound(me, kChanVoice, cry, 1, kAttnNone);
    throw_piece(q, q.s(s_gib1_mdl), health);
    throw_piece(q, q.s(s_gib2_mdl), health);
    throw_piece(q, q.s(s_gib3_mdl), health);
    become_head(q, health);
    q.set_f(me, f_act_last, 0);
    q.set_f(me, f_dead_until, q.time());
    return;
  }
  if (q.f(me, q.L.f.waterlevel) == 3) {
    q.sound(me, kChanVoice, q.s(s_h2odeath_wav), 1, kAttnNone);
    for (int i = 0; i < 5; i++) bubble_from(q, me);
  } else {
    q.sound(me, kChanVoice,
            one_of(q, {s_death1_wav, s_death2_wav, s_death3_wav, s_death4_wav, s_death5_wav}), 1,
            kAttnNone);
  }
  float first = kAxDeath;
  float frames = 9;
  if (q.f(me, q.L.f.weapon) != kWBlade) {
    float k = 1 + q.floor(q.random() * 6);
    if (k > 5) k = 5;
    const int i = static_cast<int>(k) - 1;
    first = kDeathFirst[i];
    frames = kDeathFrames[i];
  }
  quake_act(q, first, first + frames - 1, true, false);
  q.set_f(me, f_dead_until, q.time() + (frames - 1) * 0.1f);
}

// A dead player's frame: the body's frames played out; once it lies still, every button let go,
// then any pressed, he comes back.
void quake_dead_frame(A& q) {
  const int32_t me = q.self();
  // A body on the ground loses 20 u/s of its speed each command, down to a stop.
  if (band(q.f(me, q.L.f.flags), kFlOnground) != 0) {
    const Vec3 vel = q.v(me, q.L.f.velocity);
    const float speed = q.vlen(vel) - 20;
    q.set_v(me, q.L.f.velocity, speed <= 0 ? Vec3{0, 0, 0} : fv(speed, q.normalize(vel)));
  }
  quake_acting(q);
  if (q.time() < q.f(me, f_dead_until)) return;
  if (!any_button(q, me)) {
    q.set_f(me, f_freed, 1);
    return;
  }
  if (on(q.f(me, f_freed))) player_respawn(q);
}

// The four corpses a map keeps, made with the world.
void quake_corpses_make(A& q) {
  int32_t first = 0, last = 0;
  for (int i = 0; i < 4; i++) {
    const int32_t b = q.spawn();
    q.set_i(b, q.L.f.classname, q.s(s_corpse_class));
    if (first == 0) first = b;
    if (last != 0) q.set_i(last, q.L.f.owner, b);
    last = b;
  }
  q.set_i(last, q.L.f.owner, first);
  q.set_ge(g_corpse, first);
}

// The dead body `self` left where it lies, as the next of the four corpses (the oldest goes).
void quake_corpse(A& q) {
  const int32_t me = q.self();
  const int32_t b = q.ge(g_corpse);
  if (b == 0) return;
  q.set_ge(g_corpse, q.i(b, q.L.f.owner));
  q.set_v(b, q.L.f.angles, q.v(me, q.L.f.angles));
  q.set_i(b, q.L.f.model, q.i(me, q.L.f.model));
  q.set_f(b, q.L.f.modelindex, q.f(me, q.L.f.modelindex));
  q.set_f(b, q.L.f.frame, q.f(me, q.L.f.frame));
  q.set_f(b, q.L.f.colormap, q.f(me, q.L.f.colormap));
  q.set_f(b, q.L.f.movetype, q.f(me, q.L.f.movetype));
  q.set_v(b, q.L.f.velocity, q.v(me, q.L.f.velocity));
  q.set_f(b, q.L.f.flags, 0);
  q.setorigin(b, q.v(me, q.L.f.origin));
  q.setsize(b, q.v(me, q.L.f.mins), q.v(me, q.L.f.maxs));
}

// A landing: down faster than 300 at the last command's end and on the ground now: in water a
// splash; faster than 650 a hard landing that hurts 5; else a thud. Then this command's speed is
// the one the next one asks about.
void quake_land(A& q) {
  const int32_t me = q.self();
  const float was = q.f(me, f_fall_speed);
  const bool ground = band(q.f(me, q.L.f.flags), kFlOnground) != 0;
  if ((was < -300) & ground) {
    if (q.f(me, q.L.f.watertype) == kContentWater) {
      q.sound(me, kChanBody, q.s(s_h2ojump_wav), 1, kAttnNorm);
    } else if (was < -650) {
      q.set_i(me, f_deathtype, q.s(s_falling));
      damage(q, me, q.world(), q.world(), 5);
      q.sound(q.self(), kChanVoice, q.s(s_land2_wav), 1, kAttnNorm);
    } else {
      q.sound(me, kChanVoice, q.s(s_land_wav), 1, kAttnNorm);
    }
  }
  q.set_f(q.self(), f_fall_speed, q.v(q.self(), q.L.f.velocity).z);
}

// The spawn spot: a test spot when the map has one; else a deathmatch spot drawn at random from
// those no living player stands within 84 of, or from all when none is clear.
int32_t quake_spawn_spot(A& q) {
  const int32_t test = q.find(q.world(), q.L.f.classname, q.s(s_testplayerstart));
  if (test != 0) return test;
  float all = 0, clear = 0;
  for (int32_t s = q.find(q.world(), q.L.f.classname, q.s(s_info_player_deathmatch)); s != 0;
       s = q.find(s, q.L.f.classname, q.s(s_info_player_deathmatch))) {
    all = all + 1;
    bool taken = false;
    for (int32_t p = q.findradius(q.v(s, q.L.f.origin), 84); p != 0; p = q.i(p, q.L.f.chain))
      if (q.is(q.i(p, q.L.f.classname), "player") & (q.f(p, q.L.f.health) > 0)) taken = true;
    q.set_f(s, f_state, taken ? 0.0f : 1.0f);
    if (!taken) clear = clear + 1;
  }
  if (all == 0) {
    const int32_t start = q.find(q.world(), q.L.f.classname, q.s(s_info_player_start));
    if (start == 0) q.error({q.s(s_no_spawn_point)});
    return start;
  }
  const bool any = clear > 0;
  const float n = any ? clear : all;
  float pick = q.rint(q.random() * (n - 1));
  for (int32_t s = q.find(q.world(), q.L.f.classname, q.s(s_info_player_deathmatch)); s != 0;
       s = q.find(s, q.L.f.classname, q.s(s_info_player_deathmatch))) {
    if (any & (q.f(s, f_state) == 0)) continue;
    if (pick == 0) return s;
    pick = pick - 1;
  }
  return q.find(q.world(), q.L.f.classname, q.s(s_info_player_deathmatch));
}

// The flash where someone appears (a spawn, a teleport's ends), and one of five sounds there a
// fifth of a second later.
void quake_flash(A& q, Vec3 at) {
  effect_head(q, kTeTeleport);
  write_vec(q, at);
  effect_send(q, at, kMulticastPhs);
  const int32_t s = q.spawn();
  q.setorigin(s, at);
  q.set_i(s, q.L.f.think, q.fn(fn_tele_sound));
  q.set_f(s, q.L.f.nextthink, q.time() + 0.2f);
}

void tele_sound(A& q) {
  q.sound(q.self(), kChanVoice,
          one_of(q, {s_r_tele1_wav, s_r_tele2_wav, s_r_tele3_wav, s_r_tele4_wav, s_r_tele5_wav}), 1,
          kAttnNorm);
  q.remove(q.self());
}

// The kill command: refused to the dead; else told, two frags less, and back at once with the
// spawn's kit, the body left lying, no pack dropped.
void quake_kill(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.health) <= 0) {
    say_to(q, me, kPrintHigh, {q.s(s_already_dead)});
    return;
  }
  if (game_quiet(q)) return;
  say_all(q, kPrintMedium, {q.i(me, q.L.f.netname), q.s(s_suicides_nl)});
  q.set_f(me, q.L.f.frags, q.f(me, q.L.f.frags) - 2);
  note_frag(q, me, me);
  q.set_f(me, q.L.f.frame, kDeathFirst[0] + kDeathFrames[0] - 1);
  q.set_f(me, q.L.f.movetype, kMovetypeToss);
  quake_corpse(q);
  q.set_f(me, q.L.f.deadflag, kDeadRespawnable);
  q.set_gf(q.parm(1), 0);  // the default kit, nothing carried
  PutClientInServer(q);
}

// How long an item stays away in this deathmatch mode (-1: it never comes back). `kind`: 1 a
// health box, 2 the mega health, 3 armour, 4 a weapon, 5 ammunition, 6 the quad or the suit, 7
// the shield or the ring.
float quake_respawn(A& q, float kind) {
  const float dm = q.deathmatch();
  if (kind == 6) return 60;
  if (kind == 7) return 300;
  if (kind == 4) return 30;
  if (dm == 2) return -1;
  if (kind == 2) return dm == 4 ? -1.0f : 20.0f;
  if (kind == 5) return ((dm == 3) | (dm == 5)) ? 15.0f : 30.0f;
  return 20;
}

// The respawn time of the item `self` by its kind (`quake_respawn`), or `keep` for an item of no
// kind it names (the backpack, a dropped power).
float quake_item_respawn(A& q, float keep) {
  const int32_t c = q.i(q.self(), q.L.f.classname);
  float kind = 0;
  if (q.is(c, "item_health")) {
    kind = band(q.f(q.self(), q.L.f.spawnflags), 2) != 0 ? 2.0f : 1.0f;
  } else if (q.is(c, "item_armor1") | q.is(c, "item_armor2") | q.is(c, "item_armorInv")) {
    kind = 3;
  } else if (q.is(c, "item_shells") | q.is(c, "item_spikes") | q.is(c, "item_rockets") |
             q.is(c, "item_cells")) {
    kind = 5;
  } else if (q.is(c, "item_artifact_super_damage") | q.is(c, "item_artifact_envirosuit")) {
    kind = 6;
  } else if (q.is(c, "item_artifact_invulnerability") | q.is(c, "item_artifact_invisibility")) {
    kind = 7;
  } else if (q.is(c, "weapon_supershotgun") | q.is(c, "weapon_nailgun") |
             q.is(c, "weapon_supernailgun") | q.is(c, "weapon_grenadelauncher") |
             q.is(c, "weapon_rocketlauncher") | q.is(c, "weapon_lightning")) {
    kind = 4;
  }
  return kind == 0 ? keep : quake_respawn(q, kind);
}

// A key's name by the world's type: the silver or gold key, runekey or keycard.
int32_t quake_key_name(A& q, float key) {
  const float type = q.f(q.world(), f_worldtype);
  const bool silver = key == kItKey1;
  if (type == 1) return q.s(silver ? s_silver_runekey : s_gold_runekey);
  if (type == 2) return q.s(silver ? s_silver_keycard : s_gold_keycard);
  return q.s(silver ? s_w_s_key_name : s_w_g_key_name);
}

// A key lying (`item_key1` silver, `item_key2` gold) by the world's type: its model and pickup
// sound; it never comes back, and one held already is refused.
void quake_key_start(A& q, float key) {
  const int32_t me = q.self();
  const float type = q.f(q.world(), f_worldtype);
  const bool silver = key == kItKey1;
  int32_t mdl = q.s(silver ? s_w_s_key_mdl : s_w_g_key_mdl);
  int32_t snd = q.s(s_medkey_wav);
  if (type == 1) {
    mdl = q.s(silver ? s_m_s_key_mdl : s_m_g_key_mdl);
    snd = q.s(s_runekey_wav);
  } else if (type == 2) {
    mdl = q.s(silver ? s_b_s_key_mdl : s_b_g_key_mdl);
    snd = q.s(s_basekey_wav);
  }
  q.set_f(me, q.L.f.items, key);
  q.set_i(me, q.L.f.netname, quake_key_name(q, key));
  q.precache_model(mdl);
  q.precache_sound(snd);
  q.setmodel(me, mdl);
  q.set_i(me, q.L.f.noise, snd);
  q.set_f(me, q.L.f.flags, kFlItem);
  q.set_f(me, q.L.f.solid, kSolidTrigger);
  q.set_f(me, q.L.f.movetype, kMovetypeToss);
  q.setsize(me, {-16, -16, -24}, {16, 16, 32});
  q.set_v(me, q.L.f.origin, q.v(me, q.L.f.origin) + Vec3{0, 0, 6});
  q.droptofloor();
  q.set_i(me, q.L.f.touch, q.fn(fn_quake_key_touch));
}

void quake_key_touch(A& q) {
  const int32_t me = q.self();
  const int32_t o = q.other();
  if (!q.is(q.i(o, q.L.f.classname), "player") | (q.f(o, q.L.f.health) <= 0)) return;
  if (band(q.f(o, q.L.f.items), q.f(me, q.L.f.items)) != 0) return;
  say_to(q, o, kPrintLow, {q.s(s_you_got_the), q.i(me, q.L.f.netname), q.s(s_nl)});
  q.sound(o, kChanItem, q.i(me, q.L.f.noise), 1, kAttnNorm);
  q.stuffcmd(o, q.s(s_bf));
  q.set_f(o, q.L.f.items, bor(q.f(o, q.L.f.items), q.f(me, q.L.f.items)));
  q.set_ge(g_activator, o);
  use_targets(q);
  q.remove(me);
}

// What a player alive at a map change carries to the next (the spawn parms): the weapons and
// ammunition, the armour, the health held between 50 and 100 and at least 25 shells; no key or
// power. A dead one starts afresh (parm 1 is 0).
void quake_change_parms(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.health) <= 0) {
    q.set_gf(q.parm(1), 0);
    return;
  }
  const float items = q.f(me, q.L.f.items);
  const float lost =
      band(items, kItKey1 | kItKey2 | kItQuad | kItInvulnerability | kItInvisibility | kItSuit);
  float health = q.f(me, q.L.f.health);
  if (health > 100) health = 100;
  if (health < 50) health = 50;
  float shells = q.f(me, q.L.f.ammo_shells);
  if (shells < 25) shells = 25;
  q.set_gf(q.parm(1), items - lost);
  q.set_gf(q.parm(2), health);
  q.set_gf(q.parm(3), q.f(me, q.L.f.armorvalue));
  q.set_gf(q.parm(4), shells);
  q.set_gf(q.parm(5), q.f(me, q.L.f.ammo_nails));
  q.set_gf(q.parm(6), q.f(me, q.L.f.ammo_rockets));
  q.set_gf(q.parm(7), q.f(me, q.L.f.ammo_cells));
  q.set_gf(q.parm(8), q.f(me, q.L.f.weapon));
  q.set_gf(q.parm(9), q.f(me, q.L.f.armortype) * 100);
}

// The carried kit on the first spawn of a map, when there is one; false: the default kit.
bool quake_carried_kit(A& q) {
  if (q.gf(q.parm(1)) == 0) return false;
  const int32_t me = q.self();
  q.set_f(me, q.L.f.items, q.gf(q.parm(1)));
  q.set_f(me, q.L.f.health, q.gf(q.parm(2)));
  q.set_f(me, q.L.f.armorvalue, q.gf(q.parm(3)));
  q.set_f(me, q.L.f.ammo_shells, q.gf(q.parm(4)));
  q.set_f(me, q.L.f.ammo_nails, q.gf(q.parm(5)));
  q.set_f(me, q.L.f.ammo_rockets, q.gf(q.parm(6)));
  q.set_f(me, q.L.f.ammo_cells, q.gf(q.parm(7)));
  q.set_f(me, q.L.f.armortype, q.gf(q.parm(9)) / 100);
  weapon_set(q, q.gf(q.parm(8)));
  q.set_gf(q.parm(1), 0);  // the next spawn on this map starts afresh
  return true;
}

// Weapons stay where they lie, given to each who has not got one, in modes 2, 3 and 5.
bool quake_weapons_stay(A& q) {
  const float dm = q.deathmatch();
  return (dm == 2) | (dm == 3) | (dm == 5);
}

// A blast reaches `targ` when a line from it, past every player, meets the target's middle (a
// brush thing) or its origin or one of four points 15 off it across the plan.
bool quake_sees(A& q, int32_t inflictor, int32_t targ) {
  const Vec3 from = q.v(inflictor, q.L.f.origin);
  if (q.f(targ, q.L.f.movetype) == kMovetypePush) {
    q.traceline(from, (q.v(targ, q.L.f.absmin) + q.v(targ, q.L.f.absmax)) * 0.5f, 1, q.self());
    return (q.gf(q.L.g.trace_fraction) == 1) | (q.gi(q.L.g.trace_ent) == targ);
  }
  const Vec3 at = q.v(targ, q.L.f.origin);
  for (Vec3 off :
       {Vec3{0, 0, 0}, Vec3{15, 15, 0}, Vec3{-15, -15, 0}, Vec3{-15, 15, 0}, Vec3{15, -15, 0}}) {
    q.traceline(from, at + off, 1, q.self());
    if (q.gf(q.L.g.trace_fraction) == 1) return true;
  }
  return false;
}

// The best weapon to fall back on: the arc gun dry and charged, the heavy spiker with two, the
// double with two, the spiker, the scattergun, else the blade; never a launcher.
float quake_best_weapon(A& q) {
  const int32_t me = q.self();
  const float items = q.f(me, q.L.f.items);
  const float level = q.f(me, q.L.f.waterlevel);
  if ((q.f(me, q.L.f.ammo_cells) >= 1) & (band(items, kWArc) != 0) & (level <= 1)) return kWArc;
  if ((q.f(me, q.L.f.ammo_nails) >= 2) & (band(items, kWHeavySpiker) != 0)) return kWHeavySpiker;
  if ((q.f(me, q.L.f.ammo_shells) >= 2) & (band(items, kWDouble) != 0)) return kWDouble;
  if ((q.f(me, q.L.f.ammo_nails) >= 1) & (band(items, kWSpiker) != 0)) return kWSpiker;
  if ((q.f(me, q.L.f.ammo_shells) >= 1) & (band(items, kWScatter) != 0)) return kWScatter;
  return kWBlade;
}

namespace {

// The order a pickup ranks weapons in: the arc gun, the launcher, the heavy spiker, the grenade
// launcher, the double, the spiker, then the rest.
float pickup_rank(float w) {
  if (w == kWArc) return 7;
  if (w == kWRockets) return 6;
  if (w == kWHeavySpiker) return 5;
  if (w == kWGrenades) return 4;
  if (w == kWDouble) return 3;
  if (w == kWSpiker) return 2;
  return 1;
}

float slot_number(float w) {
  if (w == kWDouble) return 3;
  if (w == kWSpiker) return 4;
  if (w == kWHeavySpiker) return 5;
  if (w == kWGrenades) return 6;
  if (w == kWRockets) return 7;
  if (w == kWArc) return 8;
  return 2;
}

}  // namespace

// Whether `self` takes the weapon `w` just picked up in hand: its number within the player's own
// ceiling (the user info's `w_switch` for a weapon lying, `b_switch` for a pack; none or 0: 8),
// ranked above the one held, and never the arc gun in a liquid.
bool quake_takes_in_hand(A& q, float w, bool pack) {
  const int32_t me = q.self();
  float ceiling = number_of(q, q.infokey(me, q.s(pack ? s_b_switch : s_w_switch)));
  if (ceiling == 0) ceiling = 8;
  if (slot_number(w) > ceiling) return false;
  if ((w == kWArc) & (q.f(me, q.L.f.waterlevel) > 1)) return false;
  return pickup_rank(w) > pickup_rank(q.f(me, q.L.f.weapon));
}

// Each frame for a living player: the screen flashed each second while a power wears off, the
// quad heard on firing at most once a second (weapon_frame), the cloak's breath every 1 to 4 s.
void quake_power_frame(A& q) {
  const int32_t me = q.self();
  bool ending = false;
  for (Fd f :
       {f_super_damage_finished, f_invincible_finished, f_invisible_finished, f_suit_finished}) {
    const float until = q.f(me, f);
    if ((until != 0) & (until - 3 <= q.time())) ending = true;
  }
  if (ending & (q.time() >= q.f(me, f_warn_flash))) {
    q.stuffcmd(me, q.s(s_bf));
    q.set_f(me, f_warn_flash, q.time() + 1);
  }
  if ((q.f(me, f_invisible_finished) > q.time()) & (q.time() >= q.f(me, f_ring_heard))) {
    q.sound(me, kChanAuto, q.s(s_inv3_wav), 0.5f, kAttnIdle);
    q.set_f(me, f_ring_heard, q.time() + 1 + q.random() * 3);
  }
}

// Firing with the quad: its sound on the body, once a second at most.
void quake_quad_heard(A& q) {
  const int32_t me = q.self();
  if ((q.f(me, f_super_damage_finished) <= q.time()) | (q.time() < q.f(me, f_quad_heard))) return;
  q.sound(me, kChanBody, q.s(s_damage3_wav), 1, kAttnNorm);
  q.set_f(me, f_quad_heard, q.time() + 1);
}

void quake_precache(A& q) {
  for (St s : {s_h_player_mdl, s_gib1_mdl, s_gib2_mdl, s_gib3_mdl, s_s_bubble_spr})
    q.precache_model(q.s(s));
  for (St s : {s_teledth1_wav, s_gib_wav,     s_h2odeath_wav, s_drown1_wav,  s_drown2_wav,
               s_axhit1_wav,   s_pain1_wav,   s_pain2_wav,    s_pain3_wav,   s_pain4_wav,
               s_pain5_wav,    s_pain6_wav,   s_death1_wav,   s_death2_wav,  s_death3_wav,
               s_death4_wav,   s_death5_wav,  s_h2ojump_wav,  s_land_wav,    s_land2_wav,
               s_r_tele1_wav,  s_r_tele2_wav, s_r_tele3_wav,  s_r_tele4_wav, s_r_tele5_wav,
               s_damage3_wav,  s_inv3_wav,    s_lstart_wav,   s_gasp1_wav,   s_gasp2_wav,
               s_inh2o_wav,    s_inlava_wav,  s_lburn1_wav,   s_lburn2_wav,  s_outwater_wav,
               s_slimbrn2_wav, s_udeath_wav,  s_protect3_wav})
    q.precache_sound(q.s(s));
}

}  // namespace shooters_edge::server::rules::SE_RULES_NS
