// The arena's map things: teleport destinations, lights and flames, walls, doors, plats, buttons
// and triggers, teleporters, pushes and hurts.
//
// Doors whose boxes touch are one GROUP and open and shut together (spawnflag 4 keeps a door
// apart). The group's first door is its master, every door's .owner; .enemy chains the group
// from the master. A field round the group opens it when a player comes near, unless a door in it
// is named (a trigger opens it) or shot open (it has health); a door that needs a key (16 silver,
// 8 gold) always has its field and opens only for a player holding the key. A named group's
// words (.message) are shown to a player who touches it, every 2 s at most.
//
// ge:summary The arena's doors, plats, buttons, triggers, teleporters and lights in C++
// ge:keywords arena native-arena func_door func_plat func_button trigger_multiple teleporter
// ge:keywords trigger_push trigger_hurt door_link door-group movers
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/variant.hpp"

namespace shooters_edge::server::rules::SE_RULES_NS {

namespace {

constexpr int kDoorApart = 4;
constexpr int kDoorGold = 8;
constexpr int kDoorSilver = 16;
constexpr int kDoorToggle = 32;  // opens on one use and stays, shuts on the next


void flame_start(A& q, int32_t mdl, float fr) {
  q.precache_model(mdl);
  q.setmodel(q.self(), mdl);
  q.set_f(q.self(), q.L.f.frame, fr);
  q.makestatic(q.self());
}

}  // namespace

// A brush entity: its own model, standing still, blocking.
void brush_start(A& q) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.solid, kSolidBsp);
  q.set_f(me, q.L.f.movetype, kMovetypePush);
  q.setorigin(me, q.v(me, q.L.f.origin));
  q.setmodel(q.self(), q.i(q.self(), q.L.f.model));
}


// A trigger field the size of `self`'s brush, invisible.
void trigger_start(A& q) {
  if (!same_vec(q.v(q.self(), q.L.f.angles), {0, 0, 0})) set_movedir(q);
  const int32_t me = q.self();
  q.set_f(me, q.L.f.solid, kSolidTrigger);
  q.set_f(me, q.L.f.movetype, kMovetypeNone);
  q.setmodel(me, q.i(me, q.L.f.model));
  q.set_f(q.self(), q.L.f.modelindex, 0);
  q.set_i(q.self(), q.L.f.model, 0);
}

namespace {

// The movers' sounds: `noise1` moving, `noise2` stopped.
void mover_sounds(A& q, St a, St b) {
  q.set_i(q.self(), q.L.f.noise1, q.s(a));
  q.set_i(q.self(), q.L.f.noise2, q.s(b));
  q.precache_sound(q.s(a));
  q.precache_sound(q.s(b));
}

void door_sounds(A& q) {
  const float sounds = q.f(q.self(), q.L.f.sounds);
  if (sounds == 1) {
    mover_sounds(q, s_doormv1_wav, s_drclos4_wav);
  } else if (sounds == 2) {
    mover_sounds(q, s_hydro1_wav, s_hydro2_wav);
  } else if (sounds == 3) {
    mover_sounds(q, s_stndr1_wav, s_stndr2_wav);
  } else if (sounds == 4) {
    mover_sounds(q, s_ddoor1_wav, s_ddoor2_wav);
  } else {
    mover_sounds(q, s_null_wav, s_null_wav);
  }
  const int32_t me = q.self();
  if (band(q.f(me, q.L.f.spawnflags), 24) == 0) return;
  const float type = q.f(q.world(), f_worldtype);
  if (type == 1) {
    q.set_i(me, q.L.f.noise, q.s(s_runeuse_wav));
    q.set_i(me, q.L.f.noise3, q.s(s_runetry_wav));
  } else if (type == 2) {
    q.set_i(me, q.L.f.noise, q.s(s_baseuse_wav));
    q.set_i(me, q.L.f.noise3, q.s(s_basetry_wav));
  } else {
    q.set_i(me, q.L.f.noise, q.s(s_meduse_wav));
    q.set_i(me, q.L.f.noise3, q.s(s_medtry_wav));
  }
  q.precache_sound(q.i(me, q.L.f.noise));
  q.precache_sound(q.i(q.self(), q.L.f.noise3));
}

void plat_sounds(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.sounds) == 0) q.set_f(me, q.L.f.sounds, 2);
  if (q.f(me, q.L.f.sounds) == 1) {
    mover_sounds(q, s_plat1_wav, s_plat2_wav);
  } else {
    mover_sounds(q, s_medplat1_wav, s_medplat2_wav);
  }
}

void button_sounds(A& q) {
  const int32_t me = q.self();
  const float sounds = q.f(me, q.L.f.sounds);
  if (sounds == 1) {
    q.set_i(me, q.L.f.noise, q.s(s_switch21_wav));
  } else if (sounds == 2) {
    q.set_i(me, q.L.f.noise, q.s(s_switch02_wav));
  } else if (sounds == 3) {
    q.set_i(me, q.L.f.noise, q.s(s_switch04_wav));
  } else {
    q.set_i(me, q.L.f.noise, q.s(s_airbut1_wav));
  }
  q.precache_sound(q.i(me, q.L.f.noise));
}

// A group sounds once: its master plays for it.
void door_sound(A& q, int32_t noise) {
  if (q.i(q.self(), q.L.f.owner) == q.self()) q.sound(q.self(), kChanVoice, noise, 1, kAttnNorm);
}

void door_go_up(A& q) {
  const int32_t me = q.self();
  const float state = q.f(me, f_state);
  if (state == kStateUp) return;
  if (state == kStateTop) {  // touched again while open: wait longer
    q.set_f(me, q.L.f.nextthink, q.f(me, q.L.f.ltime) + q.f(me, f_wait));
    return;
  }
  door_sound(q, q.i(me, q.L.f.noise1));
  q.set_f(q.self(), f_state, kStateUp);
  calc_move(q, q.v(q.self(), f_pos2), q.f(q.self(), f_speed), q.fn(fn_door_hit_top));
  use_targets(q);
}

// Whether the master's group is a toggle open or opening.
bool toggle_open(A& q, int32_t master) {
  const bool toggle = band(q.f(master, q.L.f.spawnflags), kDoorToggle) != 0;
  const float state = q.f(master, f_state);
  const bool open = (state == kStateUp) | (state == kStateTop);
  return toggle & open;
}

void door_group_down(A& q) {
  const int32_t was_self = q.self();
  int32_t d = q.i(q.self(), q.L.f.owner);
  while (d != 0) {
    q.set_self(d);
    const float state = q.f(d, f_state);
    if ((state == kStateUp) | (state == kStateTop)) door_go_down(q);
    d = q.i(d, q.L.f.enemy);
  }
  q.set_self(was_self);
}

// Every door of `self`'s group opens; a toggle group open or opening shuts.
void door_group_up(A& q) {
  const int32_t owner = q.i(q.self(), q.L.f.owner);
  if (toggle_open(q, owner)) {
    door_group_down(q);
    return;
  }
  // A key group shut opens with the unlocking sound, from the door that opened it.
  {
    const bool key = band(q.f(owner, q.L.f.spawnflags), kDoorGold | kDoorSilver) != 0;
    const bool shut = q.f(owner, f_state) == kStateBottom;
    if (key & shut) q.sound(q.self(), kChanItem, q.i(owner, q.L.f.noise), 1, kAttnNorm);
  }
  const int32_t was_self = q.self();
  int32_t d = q.i(q.self(), q.L.f.owner);
  while (d != 0) {
    q.set_self(d);
    door_go_up(q);
    d = q.i(d, q.L.f.enemy);
  }
  q.set_self(was_self);
}

// `words` for `who` from the door `self`, every 2 s at most (the master keeps the time).
void door_say(A& q, int32_t who, int32_t words) {
  const int32_t owner = q.i(q.self(), q.L.f.owner);
  if (q.time() < q.f(owner, f_attack_finished)) return;
  q.set_f(owner, f_attack_finished, q.time() + 2);
  q.centerprint(who, {words});
}

// The field round a group (`lo`..`hi`) that opens it when a player comes near.
void door_spawn_field(A& q, Vec3 lo, Vec3 hi) {
  const int32_t t = q.spawn();
  q.set_i(t, q.L.f.classname, q.s(s_door_trigger));
  q.set_f(t, q.L.f.movetype, kMovetypeNone);
  q.set_f(t, q.L.f.solid, kSolidTrigger);
  q.set_i(t, q.L.f.owner, q.self());
  q.set_i(t, q.L.f.touch, q.fn(fn_door_trigger_touch));
  q.setsize(t, lo - Vec3{59, 59, 7}, hi + Vec3{59, 59, 7});
}

// Whether two things' boxes touch or overlap.
bool boxes_touch(A& q, int32_t a, int32_t b) {
  const Vec3 amin = q.v(a, q.L.f.absmin);
  const Vec3 amax = q.v(a, q.L.f.absmax);
  const Vec3 bmin = q.v(b, q.L.f.absmin);
  const Vec3 bmax = q.v(b, q.L.f.absmax);
  if ((amin.x > bmax.x) | (bmin.x > amax.x)) return false;
  if ((amin.y > bmax.y) | (bmin.y > amax.y)) return false;
  if ((amin.z > bmax.z) | (bmin.z > amax.z)) return false;
  return true;
}

void plat_spawn_field(A& q) {
  const int32_t me = q.self();
  const int32_t t = q.spawn();
  q.set_i(t, q.L.f.classname, q.s(s_plat_trigger));
  q.set_f(t, q.L.f.movetype, kMovetypeNone);
  q.set_f(t, q.L.f.solid, kSolidTrigger);
  q.set_i(t, q.L.f.owner, me);
  q.set_i(t, q.L.f.touch, q.fn(fn_plat_field_touch));
  q.setsize(t, q.v(me, f_pos2) + q.v(me, q.L.f.mins) + Vec3{25, 25, 0},
            q.v(me, f_pos2) + q.v(me, q.L.f.maxs) + Vec3{-25, -25, 8});
}

void plat_go_up(A& q) {
  q.sound(q.self(), kChanVoice, q.i(q.self(), q.L.f.noise1), 1, kAttnNorm);
  q.set_f(q.self(), f_state, kStateUp);
  calc_move(q, q.v(q.self(), f_pos1), q.f(q.self(), f_speed), q.fn(fn_plat_hit_top));
}

void button_fire(A& q) {
  const int32_t me = q.self();
  const float state = q.f(me, f_state);
  if ((state == kStateUp) | (state == kStateTop)) return;
  q.sound(me, kChanVoice, q.i(me, q.L.f.noise), 1, kAttnNorm);
  q.set_f(q.self(), f_state, kStateUp);
  calc_move(q, q.v(q.self(), f_pos2), q.f(q.self(), f_speed), q.fn(fn_button_pressed));
}

}  // namespace

void trigger_fire(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.nextthink) > q.time()) return;  // still waiting since the last time
  use_targets(q);
  const int32_t s = q.self();
  if (q.f(s, f_wait) > 0) {
    q.set_i(s, q.L.f.think, q.fn(fn_trigger_ready));
    q.set_f(s, q.L.f.nextthink, q.time() + q.f(s, f_wait));
  } else {
    // A trigger_once: done for good.
    q.set_i(s, q.L.f.touch, q.fn(fn_noop));
    q.set_f(s, q.L.f.nextthink, q.time() + 0.1f);
    q.set_i(s, q.L.f.think, q.fn(fn_remove_self));
  }
}

namespace {

// A mover's far end: `movedir` x (the brush's extent along it, less the lip).
Vec3 far_end(A& q, int32_t me) {
  const Vec3 dir = q.v(me, q.L.f.movedir);
  const Vec3 size = q.v(me, q.L.f.size);
  const float along = dir.x * size.x + dir.y * size.y + dir.z * size.z;
  return q.v(me, f_pos1) + dir * (q.fabs(along) - q.f(me, f_lip));
}

}  // namespace

void info_teleport_destination(A& q) {
  const int32_t me = q.self();
  q.set_v(me, q.L.f.origin, q.v(me, q.L.f.origin) + Vec3{0, 0, 27});
  if (q.is(q.i(me, q.L.f.targetname), "")) q.objerror({q.s(s_dest_needs_targetname)});
}

// Lights are the map compiler's; the torches and flames the clients draw where they stand.
void light(A& q) { q.remove(q.self()); }
void light_fluoro(A& q) { q.remove(q.self()); }
void light_torch_small_walltorch(A& q) { flame_start(q, q.s(s_flame_mdl), 0); }
void light_flame_large_yellow(A& q) { flame_start(q, q.s(s_flame2_mdl), 1); }
void light_flame_small_yellow(A& q) { flame_start(q, q.s(s_flame2_mdl), 0); }
void light_flame_small_white(A& q) { flame_start(q, q.s(s_flame2_mdl), 0); }
void light_globe(A& q) { flame_start(q, q.s(s_s_light_spr), 0); }

void func_wall(A& q) {
  q.set_v(q.self(), q.L.f.angles, {0, 0, 0});
  brush_start(q);
}

void func_illusionary(A& q) {
  const int32_t me = q.self();
  q.set_v(me, q.L.f.angles, {0, 0, 0});
  q.set_f(me, q.L.f.solid, kSolidNot);
  q.set_f(me, q.L.f.movetype, kMovetypeNone);
  q.setmodel(me, q.i(me, q.L.f.model));
  q.makestatic(q.self());
}

void door_hit_bottom(A& q) {
  door_sound(q, q.i(q.self(), q.L.f.noise2));
  q.set_f(q.self(), f_state, kStateBottom);
}

void door_hit_top(A& q) {
  door_sound(q, q.i(q.self(), q.L.f.noise2));
  const int32_t me = q.self();
  q.set_f(me, f_state, kStateTop);
  const bool forever = q.f(me, f_wait) < 0;
  const bool toggle = band(q.f(q.i(me, q.L.f.owner), q.L.f.spawnflags), kDoorToggle) != 0;
  if (forever | toggle) return;  // stays open
  q.set_i(me, q.L.f.think, q.fn(fn_door_go_down));
  q.set_f(me, q.L.f.nextthink, q.f(me, q.L.f.ltime) + q.f(me, f_wait));
}

void door_go_down(A& q) {
  door_sound(q, q.i(q.self(), q.L.f.noise1));
  q.set_f(q.self(), f_state, kStateDown);
  calc_move(q, q.v(q.self(), f_pos1), q.f(q.self(), f_speed), q.fn(fn_door_hit_bottom));
}

// Something stands in the way: it takes the door's damage and the group goes back.
void door_blocked(A& q) {
  q.set_i(q.other(), f_deathtype, q.s(s_squished));
  damage(q, q.other(), q.self(), q.self(), q.f(q.self(), f_dmg));
  if (q.f(q.self(), f_wait) >= 0) {
    if (q.f(q.self(), f_state) == kStateDown) {
      door_group_up(q);
    } else {
      door_group_down(q);
    }
  }
}

void door_use(A& q) { door_group_up(q); }

// A blow that takes a shot door's health opens its group; it can be shot again.
void door_killed(A& q) {
  q.set_f(q.self(), q.L.f.health, q.f(q.self(), q.L.f.max_health));
  q.set_ge(g_activator, q.ge(g_damage_attacker));
  door_group_up(q);
}

// A named group's door touched by a player: its words, if it has any.
void door_touch(A& q) {
  const int32_t o = q.other();
  if (!q.is(q.i(o, q.L.f.classname), "player") | (q.f(o, q.L.f.health) <= 0)) return;
  const int32_t owner = q.i(q.self(), q.L.f.owner);
  if (q.is(q.i(owner, q.L.f.netname), "")) return;
  door_say(q, q.other(), q.i(owner, q.L.f.netname));
}

// A player at a group's field, or at a key group's door: it opens, or says which key it needs.
void door_trigger_touch(A& q) {
  const int32_t o = q.other();
  if ((q.f(o, q.L.f.health) <= 0) | !q.is(q.i(o, q.L.f.classname), "player")) return;
  const int32_t field = q.self();
  q.set_self(q.i(field, q.L.f.owner));
  const int32_t me = q.self();
  float key = 0;
  if (on(band(q.f(me, q.L.f.spawnflags), kDoorSilver))) {
    key = kItKey1;
  } else if (on(band(q.f(me, q.L.f.spawnflags), kDoorGold))) {
    key = kItKey2;
  }
  if ((key != 0) & (band(q.f(q.other(), q.L.f.items), key) == 0)) {
    if (q.time() >= q.f(me, f_attack_finished))
      q.sound(field, kChanItem, q.i(me, q.L.f.noise3), 1, kAttnNorm);
    if (key == kItKey1) {
      door_say(q, q.other(), q.s(s_needs_silver_key));
    } else {
      door_say(q, q.other(), q.s(s_needs_gold_key));
    }
  } else {
    q.set_ge(g_activator, q.other());
    door_group_up(q);
  }
  q.set_self(field);
}

// Once every brush has its size, the first door of a group to think gathers every door touching
// it (and those touching them), then gives the group its field or its words.
void door_link(A& q) {
  const int32_t me = q.self();
  if (q.i(me, q.L.f.owner) != me) return;  // already in another door's group
  int32_t tail = me;
  bool grew = band(q.f(me, q.L.f.spawnflags), kDoorApart) == 0;
  while (grew) {
    grew = false;
    int32_t d = q.find(q.world(), q.L.f.classname, q.s(s_func_door));
    while (d != 0) {
      if ((q.i(d, q.L.f.owner) == d) & (d != me) & (q.i(d, q.L.f.enemy) == 0) &
          (band(q.f(d, q.L.f.spawnflags), kDoorApart) == 0)) {
        int32_t m = me;
        while (m != 0) {
          if (boxes_touch(q, m, d)) {
            q.set_i(tail, q.L.f.enemy, d);
            tail = d;
            q.set_i(d, q.L.f.owner, me);
            grew = true;
            m = q.world();
          } else {
            m = q.i(m, q.L.f.enemy);
          }
        }
      }
      d = q.find(d, q.L.f.classname, q.s(s_func_door));
    }
  }
  bool named = false;
  bool shot = false;
  Vec3 lo = q.v(me, q.L.f.absmin);
  Vec3 hi = q.v(me, q.L.f.absmax);
  for (int32_t d = me; d != 0; d = q.i(d, q.L.f.enemy)) {
    q.set_f(
        me, q.L.f.spawnflags,
        bor(q.f(me, q.L.f.spawnflags), band(q.f(d, q.L.f.spawnflags), kDoorGold | kDoorSilver)));
    if (!q.is(q.i(d, q.L.f.targetname), "")) named = true;
    if (on(q.f(d, q.L.f.max_health))) shot = true;
    const Vec3 dmin = q.v(d, q.L.f.absmin);
    const Vec3 dmax = q.v(d, q.L.f.absmax);
    if (dmin.x < lo.x) lo.x = dmin.x;
    if (dmin.y < lo.y) lo.y = dmin.y;
    if (dmin.z < lo.z) lo.z = dmin.z;
    if (dmax.x > hi.x) hi.x = dmax.x;
    if (dmax.y > hi.y) hi.y = dmax.y;
    if (dmax.z > hi.z) hi.z = dmax.z;
  }
  const bool keyed = band(q.f(me, q.L.f.spawnflags), kDoorGold | kDoorSilver) != 0;
  if (keyed | (!named & !shot)) {
    door_spawn_field(q, lo, hi);
    // A key group's doors answer a touch themselves too: a player who walks into one hears it.
    int32_t d = me;
    while ((d != 0) & (band(q.f(me, q.L.f.spawnflags), kDoorGold | kDoorSilver) != 0)) {
      q.set_i(d, q.L.f.touch, q.fn(fn_door_trigger_touch));
      d = q.i(d, q.L.f.enemy);
    }
    return;
  }
  // A named group shows its words on a touch, never as it opens.
  for (int32_t d = me; d != 0; d = q.i(d, q.L.f.enemy)) {
    if (q.is(q.i(me, q.L.f.netname), "")) q.set_i(me, q.L.f.netname, q.i(d, q.L.f.message));
    q.set_i(d, q.L.f.message, 0);
    q.set_i(d, q.L.f.touch, q.fn(fn_door_touch));
  }
}

void func_door(A& q) {
  set_movedir(q);
  brush_start(q);
  const int32_t me = q.self();
  q.set_i(me, q.L.f.blocked, q.fn(fn_door_blocked));
  q.set_i(me, q.L.f.use, q.fn(fn_door_use));
  if (q.f(me, f_speed) == 0) q.set_f(me, f_speed, 100);
  if (q.f(me, f_wait) == 0) q.set_f(me, f_wait, 3);
  if (q.f(me, f_lip) == 0) q.set_f(me, f_lip, 8);
  if (q.f(me, f_dmg) == 0) q.set_f(me, f_dmg, 2);
  q.set_v(me, f_pos1, q.v(me, q.L.f.origin));
  q.set_v(me, f_pos2, far_end(q, me));
  if (on(band(q.f(me, q.L.f.spawnflags), 1))) {
    // Starts open: it stands where it would open to, and "opening" it shuts it.
    q.setorigin(me, q.v(me, f_pos2));
    q.set_v(q.self(), f_pos2, q.v(q.self(), f_pos1));
    q.set_v(q.self(), f_pos1, q.v(q.self(), q.L.f.origin));
  }
  const int32_t s = q.self();
  q.set_f(s, f_state, kStateBottom);
  q.set_i(s, q.L.f.owner, s);  // alone until door_link gathers it
  door_sounds(q);
  const int32_t t = q.self();
  if (on(q.f(t, q.L.f.health))) {
    q.set_f(t, q.L.f.max_health, q.f(t, q.L.f.health));
    q.set_f(t, q.L.f.takedamage, kDamageYes);
    q.set_i(t, f_th_hurt, q.fn(fn_noop));
    q.set_i(t, f_th_die, q.fn(fn_door_killed));
  }
  q.set_i(t, q.L.f.think, q.fn(fn_door_link));
  q.set_f(t, q.L.f.nextthink, q.f(t, q.L.f.ltime) + 0.1f);  // after every brush has its size
}

void plat_hit_top(A& q) {
  const int32_t me = q.self();
  q.sound(me, kChanVoice, q.i(me, q.L.f.noise2), 1, kAttnNorm);
  q.set_f(me, f_state, kStateTop);
  q.set_i(me, q.L.f.think, q.fn(fn_plat_go_down));
  q.set_f(me, q.L.f.nextthink, q.f(me, q.L.f.ltime) + 3);
}

void plat_hit_bottom(A& q) {
  q.sound(q.self(), kChanVoice, q.i(q.self(), q.L.f.noise2), 1, kAttnNorm);
  q.set_f(q.self(), f_state, kStateBottom);
}

void plat_go_down(A& q) {
  q.sound(q.self(), kChanVoice, q.i(q.self(), q.L.f.noise1), 1, kAttnNorm);
  q.set_f(q.self(), f_state, kStateDown);
  calc_move(q, q.v(q.self(), f_pos2), q.f(q.self(), f_speed), q.fn(fn_plat_hit_bottom));
}

void plat_blocked(A& q) {
  q.set_i(q.other(), f_deathtype, q.s(s_squished));
  damage(q, q.other(), q.self(), q.self(), 1);
  const float state = q.f(q.self(), f_state);
  if (state == kStateUp) {
    plat_go_down(q);
  } else if (state == kStateDown) {
    plat_go_up(q);
  }
}

// The field on top of a lowered plat: a player standing on it rides it up.
void plat_field_touch(A& q) {
  const int32_t o = q.other();
  if (!q.is(q.i(o, q.L.f.classname), "player") | (q.f(o, q.L.f.health) <= 0)) return;
  const int32_t field = q.self();
  q.set_self(q.i(field, q.L.f.owner));
  const float state = q.f(q.self(), f_state);
  if (state == kStateBottom) {
    plat_go_up(q);
  } else if (state == kStateTop) {
    q.set_f(q.self(), q.L.f.nextthink, q.f(q.self(), q.L.f.ltime) + 1);
  }
  q.set_self(field);
}

// A named plat waits at its top until it is used; then it is an ordinary plat.
void plat_use(A& q) {
  if (q.f(q.self(), f_state) != kStateWaiting) return;
  plat_go_down(q);
}

void func_plat(A& q) {
  brush_start(q);
  plat_sounds(q);
  const int32_t me = q.self();
  q.set_i(me, q.L.f.blocked, q.fn(fn_plat_blocked));
  if (q.f(me, f_speed) == 0) q.set_f(me, f_speed, 150);
  q.set_v(me, f_pos1, q.v(me, q.L.f.origin));  // the top: where the map put it
  if (on(q.f(me, f_height))) {
    q.set_v(me, f_pos2, q.v(me, f_pos1) - Vec3{0, 0, 1} * q.f(me, f_height));
  } else {
    q.set_v(me, f_pos2, q.v(me, f_pos1) - Vec3{0, 0, 1} * (q.v(me, q.L.f.size).z - 8));
  }
  if (!q.is(q.i(me, q.L.f.targetname), "")) {
    q.set_f(me, f_state, kStateWaiting);
    q.set_i(me, q.L.f.use, q.fn(fn_plat_use));
  } else {
    q.setorigin(me, q.v(me, f_pos2));
    q.set_f(q.self(), f_state, kStateBottom);
  }
  plat_spawn_field(q);
}

void button_return(A& q) {
  q.set_f(q.self(), f_state, kStateDown);
  calc_move(q, q.v(q.self(), f_pos1), q.f(q.self(), f_speed), q.fn(fn_noop));
  q.set_f(q.self(), q.L.f.frame, 0);
  if (on(q.f(q.self(), q.L.f.max_health)))
    q.set_f(q.self(), q.L.f.takedamage, kDamageYes);  // a shot one can be shot again
}

void button_pressed(A& q) {
  const int32_t me = q.self();
  q.set_f(me, f_state, kStateTop);
  q.set_f(me, q.L.f.frame, 1);
  q.set_i(me, q.L.f.think, q.fn(fn_button_return));
  q.set_f(me, q.L.f.nextthink, q.f(me, q.L.f.ltime) + q.f(me, f_wait));
  use_targets(q);
}

void button_touch(A& q) {
  if (!q.is(q.i(q.other(), q.L.f.classname), "player")) return;
  q.set_ge(g_activator, q.other());
  button_fire(q);
}

void button_use(A& q) {
  q.set_ge(g_activator, q.other());
  button_fire(q);
}

// A button the map gives health is pressed by a blow that takes it.
void button_killed(A& q) {
  q.set_ge(g_activator, q.ge(g_damage_attacker));
  q.set_f(q.self(), q.L.f.health, q.f(q.self(), q.L.f.max_health));
  q.set_f(q.self(), q.L.f.takedamage, kDamageNo);
  button_fire(q);
}

void func_button(A& q) {
  set_movedir(q);
  brush_start(q);
  button_sounds(q);
  const int32_t me = q.self();
  q.set_i(me, q.L.f.touch, q.fn(fn_button_touch));
  q.set_i(me, q.L.f.use, q.fn(fn_button_use));
  if (q.f(me, f_speed) == 0) q.set_f(me, f_speed, 40);
  if (q.f(me, f_wait) == 0) q.set_f(me, f_wait, 1);
  if (q.f(me, f_lip) == 0) q.set_f(me, f_lip, 4);
  q.set_v(me, f_pos1, q.v(me, q.L.f.origin));
  q.set_v(me, f_pos2, far_end(q, me));
  q.set_f(me, f_state, kStateBottom);
  if (on(q.f(me, q.L.f.health))) {
    q.set_f(me, q.L.f.max_health, q.f(me, q.L.f.health));
    q.set_f(me, q.L.f.takedamage, kDamageYes);
    q.set_i(me, f_th_die, q.fn(fn_button_killed));
    q.set_i(me, q.L.f.touch, q.fn(fn_noop));  // only a blow presses it
  }
}

void trigger_ready(A& q) { q.set_f(q.self(), q.L.f.solid, kSolidTrigger); }

void trigger_touch(A& q) {
  const int32_t o = q.other();
  if (!q.is(q.i(o, q.L.f.classname), "player") | (q.f(o, q.L.f.health) <= 0)) return;
  q.set_ge(g_activator, q.other());
  trigger_fire(q);
}

void trigger_use(A& q) {
  q.set_ge(g_activator, q.other());
  trigger_fire(q);
}

void trigger_multiple(A& q) {
  if (q.f(q.self(), f_wait) == 0) q.set_f(q.self(), f_wait, 0.2f);
  trigger_start(q);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_trigger_touch));
  q.set_i(q.self(), q.L.f.use, q.fn(fn_trigger_use));
}

void trigger_once(A& q) {
  q.set_f(q.self(), f_wait, -1);
  trigger_start(q);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_trigger_touch));
  q.set_i(q.self(), q.L.f.use, q.fn(fn_trigger_use));
}

// Sends what the variant lets it to the destination named by .target, even a moving one.
void teleport_touch(A& q) {
  if (!teleport_takes(q, q.other())) return;
  const int32_t dest = q.find(q.world(), q.L.f.targetname, q.i(q.self(), q.L.f.target));
  if (dest == 0) {
    q.objerror({q.s(s_teleporter_no_destination), q.i(q.self(), q.L.f.target)});
    return;
  }
  q.makevectors(q.v(dest, q.L.f.angles));
  const int32_t o = q.other();
  q.setorigin(o, q.v(dest, q.L.f.origin));
  q.set_v(o, q.L.f.angles, q.v(dest, q.L.f.angles));
  q.set_f(o, q.L.f.fixangle, 1);
  q.set_v(o, q.L.f.velocity, q.v_forward() * 300);
  q.set_f(o, q.L.f.teleport_time, q.time() + 0.7f);
  const float flags = q.f(o, q.L.f.flags);
  q.set_f(o, q.L.f.flags, flags - band(flags, kFlOnground));
  q.sound(o, kChanVoice, q.s(s_r_tele1_wav), 1, kAttnNorm);
  teleport_arrived(q, q.other());
}

void trigger_teleport(A& q) {
  trigger_start(q);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_teleport_touch));
  if (q.is(q.i(q.self(), q.L.f.target), "")) q.objerror({q.s(s_teleporter_needs_target)});
}

// Pushes whatever is in it along its angle at .speed (1000 when not set).
void push_touch(A& q) {
  const int32_t o = q.other();
  if ((q.f(o, q.L.f.health) <= 0) | (q.f(o, q.L.f.movetype) == kMovetypeNone)) return;
  q.set_v(o, q.L.f.velocity, q.v(q.self(), q.L.f.movedir) * q.f(q.self(), f_speed) * 10);
}

void trigger_push(A& q) {
  trigger_start(q);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_push_touch));
  if (q.f(q.self(), f_speed) == 0) q.set_f(q.self(), f_speed, 1000);
}

// Hurts whatever is in it by .dmg (5 when not set) once a second.
void hurt_ready(A& q) { q.set_f(q.self(), q.L.f.solid, kSolidTrigger); }

void hurt_touch(A& q) {
  if (q.f(q.other(), q.L.f.takedamage) == 0) return;
  q.set_f(q.self(), q.L.f.solid, kSolidNot);
  q.set_i(q.other(), f_deathtype, q.s(s_hurt));
  damage(q, q.other(), q.self(), q.self(), q.f(q.self(), f_dmg));
  q.set_i(q.self(), q.L.f.think, q.fn(fn_hurt_ready));
  q.set_f(q.self(), q.L.f.nextthink, q.time() + 1);
}

void trigger_hurt(A& q) {
  trigger_start(q);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_hurt_touch));
  if (q.f(q.self(), f_dmg) == 0) q.set_f(q.self(), f_dmg, 5);
}

}  // namespace shooters_edge::server::rules::SE_RULES_NS
