// The monsters' minds: a monster's life (spawning, being used, dying), its senses (the four
// ranges, seeing, in front, noticing a player) and its states (stand, walk a path, run at an
// enemy, attack, pain). Each kind (campaign_monsters*.cpp) fills in its own numbers and attacks:
// the `th_` hooks, its step distances, its pain and its death.
//
// A state is a think that runs every 0.1 s and takes one decision with the kind's step distance.
// A death: the body plays one of the kind's death sequences (two for some kinds, half the time
// each) and lies on its last frame for good, solid for its first few frames, a backpack dropped on
// the third by the kinds that carry ammunition; below the kind's gib health it bursts instead, the
// body turned into its head and its pieces thrown at a speed set by how far below zero the health
// went.
//
// ge:summary The campaign's monster minds in C++: senses, states, attacks, pain, death, gibs
// ge:keywords campaign native-campaign monster-ai look_for_target check_attack ai_run
// ge:keywords monster_die gibs corpse_fall path_corner monster_missile walkmonster_start
// ge:skill progs shooter bots
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/campaign_decl.hpp"

namespace shooters_edge::server::rules::campaign {

namespace {

Vec3 eye(A& q, int32_t e) { return q.v(e, q.L.f.origin) + q.v(e, q.L.f.view_ofs); }

bool is_player(A& q, int32_t e) { return q.is(q.i(e, q.L.f.classname), "player"); }

void sees_on(A& q) { q.set_f(q.self(), f_search_time, q.time() + 5); }

// The enemy dead: back to the one it turned from, else to its path, else it stands.
void lost_enemy(A& q) {
  const int32_t me = q.self();
  q.set_i(me, q.L.f.enemy, q.world());
  const int32_t old = q.i(me, f_oldenemy);
  if ((q.f(old, q.L.f.health) > 0) & (q.f(old, q.L.f.takedamage) != 0)) {
    q.set_i(me, q.L.f.enemy, old);
    q.set_i(me, f_oldenemy, q.world());
    found_target(q);
    return;
  }
  if (q.i(me, f_movetarget) != 0) {
    q.set_i(me, q.L.f.goalentity, q.i(me, f_movetarget));
    run_field(q, me, f_th_walk);
  } else {
    q.set_f(me, f_pausetime, q.time() + kForever);
    run_field(q, me, f_th_stand);
  }
}

void ai_stand(A& q) {
  if (look_for_target(q)) return;
  if (q.time() > q.f(q.self(), f_pausetime)) run_field(q, q.self(), f_th_walk);
}

void ai_walk(A& q, float dist) {
  if (look_for_target(q)) return;
  q.movetogoal(dist);
}

void ai_run(A& q, float dist) {
  const int32_t me = q.self();
  const int32_t enemy = q.i(me, q.L.f.enemy);
  if ((q.f(enemy, q.L.f.health) <= 0) | (q.f(enemy, q.L.f.takedamage) == 0)) {
    lost_enemy(q);
    return;
  }
  if (can_see(q, enemy)) sees_on(q);
  if ((q.coop() != 0) & (q.f(me, f_search_time) < q.time())) {
    if (look_for_target(q)) return;
  }
  if (q.f(me, f_attack_state) == kAsMissile) {
    if (face_enemy(q)) {
      q.set_f(me, f_attack_state, kAsStraight);
      run_field(q, me, f_th_missile);
    }
    return;
  }
  if (q.f(me, f_attack_state) == kAsMelee) {
    if (face_enemy(q)) {
      q.set_f(me, f_attack_state, kAsStraight);
      run_field(q, me, f_th_melee);
    }
    return;
  }
  float decided = 0;
  if (q.i(me, f_th_check) != 0) {
    run_field(q, me, f_th_check);
    decided = pg::result_float(q.vm);
  } else {
    decided = check_attack(q) ? 1 : 0;
  }
  if (on(decided)) return;
  q.movetogoal(dist);
}

// A piece's speed from the health its body died on (`dm`, below zero): up to 100 each way across
// and 200 to 300 up, times 0.7 above -50, 2 down to -200, 10 below that (a body torn apart).
Vec3 gib_velocity(A& q, float dm) {
  const float c1 = crandom(q) * 100;
  Vec3 v = fv(c1, {1, 0, 0});
  const float c2 = crandom(q) * 100;
  v = v + fv(c2, {0, 1, 0});
  const float up = 200 + q.random() * 100;
  v = v + fv(up, {0, 0, 1});
  if (dm > -50) return v * 0.7f;
  if (dm > -200) return v * 2;
  return v * 10;
}

Vec3 spin(A& q) {
  const float a = q.random() * 600;
  Vec3 v = fv(a, {1, 0, 0});
  const float b = q.random() * 600;
  v = v + fv(b, {0, 1, 0});
  const float c = q.random() * 600;
  return v + fv(c, {0, 0, 1});
}

void throw_gib(A& q, int32_t mdl, float dm) {
  throw_gib_at(q, mdl, q.v(q.self(), q.L.f.origin), dm);
}

// The kind's pieces from where the body is now.
void throw_pieces(A& q) {
  const int32_t me = q.self();
  if (!q.is(q.i(me, f_gib_model), "")) {
    throw_gib(q, q.i(me, f_gib_model), q.f(me, q.L.f.health));
    throw_gib(q, q.i(me, f_gib_model), q.f(me, q.L.f.health));
    throw_gib(q, q.i(me, f_gib_model), q.f(me, q.L.f.health));
    return;
  }
  throw_gib(q, q.s(s_gib1_mdl), q.f(me, q.L.f.health));
  throw_gib(q, q.s(s_gib2_mdl), q.f(me, q.L.f.health));
  throw_gib(q, q.s(s_gib3_mdl), q.f(me, q.L.f.health));
}

// The body itself becomes its head: 24 lower, thrown as a piece is, spinning about its height,
// lying where it lands for good.
void become_head(A& q, int32_t mdl) {
  const int32_t me = q.self();
  q.setmodel(me, mdl);
  q.set_f(me, q.L.f.frame, 0);
  q.set_f(me, q.L.f.movetype, kMovetypeBounce);
  q.set_f(me, q.L.f.takedamage, kDamageNo);
  q.set_f(me, q.L.f.solid, kSolidNot);
  q.set_v(me, q.L.f.view_ofs, {0, 0, 8});
  q.setsize(me, {-16, -16, 0}, {16, 16, 56});
  q.set_v(me, q.L.f.velocity, gib_velocity(q, q.f(me, q.L.f.health)));
  q.setorigin(me, q.v(me, q.L.f.origin) - Vec3{0, 0, 24});
  const float flags = q.f(me, q.L.f.flags);
  q.set_f(me, q.L.f.flags, flags - band(flags, kFlOnground));
  const float c = crandom(q) * 600;
  q.set_v(me, q.L.f.avelocity, fv(c, {0, 1, 0}));
}

void set_think(A& q, int32_t e, Fn fn, float at) {
  q.set_i(e, q.L.f.think, q.fn(fn));
  q.set_f(e, q.L.f.nextthink, at);
}

}  // namespace

// THE SENSES.
float range_to(A& q, int32_t targ) {
  const float r = q.vlen(eye(q, q.self()) - eye(q, targ));
  if (r < 120) return kRangeMelee;
  if (r < 500) return kRangeNear;
  if (r < 1000) return kRangeMid;
  return kRangeFar;
}

// A line from eye to eye that ignores monsters, and does not cross into or out of water.
bool can_see(A& q, int32_t targ) {
  q.traceline(eye(q, q.self()), eye(q, targ), 1, q.self());
  if ((q.gf(q.L.g.trace_inopen) != 0) & (q.gf(q.L.g.trace_inwater) != 0)) return false;
  return q.gf(q.L.g.trace_fraction) == 1;
}

// Within about 73 degrees either side of where the monster faces.
bool in_front(A& q, int32_t targ) {
  const int32_t me = q.self();
  q.makevectors(q.v(me, q.L.f.angles));
  return dot(q.normalize(q.v(targ, q.L.f.origin) - q.v(me, q.L.f.origin)), q.v_forward()) > 0.3f;
}

// The yaw toward the enemy, and whether the monster now faces it within 45 degrees.
bool face_enemy(A& q) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.ideal_yaw,
          q.vectoyaw(q.v(q.i(me, q.L.f.enemy), q.L.f.origin) - q.v(me, q.L.f.origin)));
  q.changeyaw();
  float delta = q.v(me, q.L.f.angles).y - q.f(me, q.L.f.ideal_yaw);
  while (delta < 0) delta = delta + 360;
  while (delta >= 360) delta = delta - 360;
  return (delta <= 45) | (delta >= 315);
}

// The monster has an enemy: it calls out, tells the room, heads for him and holds its fire a second.
void found_target(A& q) {
  const int32_t me = q.self();
  const int32_t enemy = q.i(me, q.L.f.enemy);
  if (is_player(q, enemy)) {
    q.set_ge(g_sight_entity, me);
    q.set_g(g_sight_entity_time, q.time());
  }
  q.set_f(me, f_show_hostile, q.time() + 1);
  q.set_i(me, q.L.f.goalentity, q.i(me, q.L.f.enemy));
  q.set_f(me, q.L.f.ideal_yaw,
          q.vectoyaw(q.v(q.i(me, q.L.f.enemy), q.L.f.origin) - q.v(me, q.L.f.origin)));
  q.set_f(me, f_search_time, q.time() + 5);
  q.set_f(me, f_attack_finished, q.time() + 1);
  q.set_f(me, f_attack_state, kAsStraight);
  if (!q.is(q.i(me, q.L.f.noise), ""))
    q.sound(me, kChanVoice, q.i(me, q.L.f.noise), 1, kAttnNorm);
  run_field(q, me, f_th_run);
}

// Once a frame: one candidate, the client the server offers or a monster that has just spotted
// one, judged by its range: far never, mid in front and seen, near seen and in front unless he
// made noise, melee seen.
bool look_for_target(A& q) {
  const int32_t me = q.self();
  int32_t client = 0;
  if ((q.g(g_sight_entity_time) >= q.time() - 0.1f) &
      (band(q.f(me, q.L.f.spawnflags), kSpawnAmbush) == 0)) {
    client = q.ge(g_sight_entity);
    if (q.i(client, q.L.f.enemy) == q.i(me, q.L.f.enemy)) return false;
  } else {
    client = q.checkclient();
    if (client == 0) return false;
  }
  if (client == q.i(me, q.L.f.enemy)) return false;
  if (on(band(q.f(client, q.L.f.flags), kFlNotarget))) return false;
  const float r = range_to(q, client);
  if (r == kRangeFar) return false;
  if (!can_see(q, client)) return false;
  if (r == kRangeNear) {
    const bool quiet = q.f(client, f_show_hostile) < q.time();
    const bool ahead = in_front(q, client);  // asked either way
    if (quiet & !ahead) return false;
  } else if (r == kRangeMid) {
    if (!in_front(q, client)) return false;
  }
  q.set_i(me, q.L.f.enemy, client);
  if (!is_player(q, q.i(me, q.L.f.enemy))) {
    q.set_i(me, q.L.f.enemy, q.i(q.i(me, q.L.f.enemy), q.L.f.enemy));  // a monster passes it on
    if (!is_player(q, q.i(me, q.L.f.enemy))) {
      q.set_i(me, q.L.f.enemy, q.world());
      return false;
    }
  }
  found_target(q);
  return true;
}

// THE DECISION TO ATTACK, the default: a clear line to the enemy, then the melee blow in melee
// range, else a ranged attack by chance when its cooldown has passed and the enemy is not far.
bool check_attack(A& q) {
  const int32_t me = q.self();
  const int32_t enemy = q.i(me, q.L.f.enemy);
  q.traceline(eye(q, me), eye(q, enemy), 0, me);
  if ((q.gf(q.L.g.trace_inopen) != 0) & (q.gf(q.L.g.trace_inwater) != 0)) return false;
  if (q.gi(q.L.g.trace_ent) != q.i(me, q.L.f.enemy)) return false;
  const float r = range_to(q, q.i(me, q.L.f.enemy));
  if ((r == kRangeMelee) & (q.i(me, f_th_melee) != 0)) {
    q.set_f(me, f_attack_state, kAsMelee);
    return true;
  }
  if (q.i(me, f_th_missile) == 0) return false;
  if (q.time() < q.f(me, f_attack_finished)) return false;
  if (r == kRangeFar) return false;
  const bool melee = q.i(me, f_th_melee) != 0;
  float chance = 0;
  if (r == kRangeMelee) chance = 0.9f;
  else if (r == kRangeNear) chance = melee ? 0.2f : 0.4f;
  else chance = melee ? 0.05f : 0.1f;
  if (q.random() < chance) {
    q.set_f(me, f_attack_state, kAsMissile);
    q.set_f(me, f_attack_finished, q.time() + 2 * q.random());
    return true;
  }
  return false;
}

// A melee blow: within 60 units of the enemy and seen, `n` random numbers summed times `factor`.
void melee_blow_n(A& q, float n, float factor) {
  const int32_t me = q.self();
  if (q.vlen(q.v(q.i(me, q.L.f.enemy), q.L.f.origin) - q.v(me, q.L.f.origin)) > 60) return;
  if (!can_see(q, q.i(me, q.L.f.enemy))) return;
  float sum = 0;
  while (n > 0) {
    sum = sum + q.random();
    n = n - 1;
  }
  q.set_i(q.i(me, q.L.f.enemy), f_deathtype, q.s(s_mauled));
  damage(q, q.i(me, q.L.f.enemy), me, me, sum * factor);
}

// The usual blow: three random numbers.
void melee_blow(A& q, float factor) { melee_blow_n(q, 3, factor); }

// A step of a melee attack: facing the enemy, closing in by `dist` until within the blow's reach.
void ai_charge(A& q, float dist) {
  face_enemy(q);
  const int32_t me = q.self();
  if (q.vlen(q.v(q.i(me, q.L.f.enemy), q.L.f.origin) - q.v(me, q.L.f.origin)) > 60)
    q.walkmove(q.f(me, q.L.f.ideal_yaw), dist);
}

// THE FRAMES. The next frame of a loop, from its first when the monster was showing another.
void frame_loop(A& q, Vec3 r) {
  if (r.z == 0) return;
  const int32_t me = q.self();
  const float fr = q.f(me, q.L.f.frame);
  if ((fr < r.x) | (fr >= r.y)) q.set_f(me, q.L.f.frame, r.x);
  else q.set_f(me, q.L.f.frame, fr + 1);
}

// An attack, a leap or a pain begins: its frames from the first.
void anim_begin(A& q, Vec3 r) {
  q.set_f(q.self(), f_anim_step, 0);
  q.set_v(q.self(), f_fr_now, r);
}

// One 0.1 s step further into it, and the frame that step shows (the last held).
void anim_tick(A& q) {
  const int32_t me = q.self();
  q.set_f(me, f_anim_step, q.f(me, f_anim_step) + 1);
  const Vec3 now = q.v(me, f_fr_now);
  if (now.z == 0) return;
  float f = now.x + q.f(me, f_anim_step) - 1;
  if (f > now.y) f = now.y;
  q.set_f(me, q.L.f.frame, f);
}

// THE STATES as thinks: one step every 0.1 s at the kind's distances.
void monster_stand(A& q) {
  const int32_t me = q.self();
  set_think(q, me, fn_monster_stand, q.time() + 0.1f);
  frame_loop(q, q.v(me, f_fr_stand));
  ai_stand(q);
}

void monster_walk(A& q) {
  const int32_t me = q.self();
  set_think(q, me, fn_monster_walk, q.time() + 0.1f);
  frame_loop(q, q.v(me, f_fr_walk));
  ai_walk(q, q.f(me, f_walk_step));
}

void monster_run(A& q) {
  const int32_t me = q.self();
  set_think(q, me, fn_monster_run, q.time() + 0.1f);
  frame_loop(q, q.v(me, f_fr_run));
  ai_run(q, q.f(me, f_run_step));
}

// BEING HURT. By anything but itself or its enemy, it turns on the attacker, unless he is of its
// own kind (riflemen excepted); a player turned from is remembered. Then the kind's pain, at the
// hardest skill no more than every 5 s.
void monster_hurt(A& q) {
  const int32_t me = q.self();
  const int32_t a = q.ge(g_damage_attacker);
  if ((a != me) & (a != q.i(me, q.L.f.enemy)) & (q.f(a, q.L.f.takedamage) != 0) &
      (band(q.f(a, q.L.f.flags), static_cast<float>(kFlClient | kFlMonster)) != 0)) {
    if (!q.same(q.i(a, q.L.f.classname), q.i(me, q.L.f.classname)) |
        q.is(q.i(me, q.L.f.classname), "monster_army")) {
      if (is_player(q, q.i(me, q.L.f.enemy))) q.set_i(me, f_oldenemy, q.i(me, q.L.f.enemy));
      q.set_i(me, q.L.f.enemy, a);
      found_target(q);
    }
  }
  if (q.time() < q.f(me, f_pain_finished)) return;
  if (q.cvar(q.s(s_skill)) >= 3) q.set_f(me, f_pain_finished, q.time() + 5);
  if (q.i(me, f_th_pain) != 0) run_field(q, me, f_th_pain);
}

// Fired by a trigger: angry at the player who set it off.
void monster_use(A& q) {
  const int32_t me = q.self();
  if (q.i(me, q.L.f.enemy) != 0) return;
  if (q.f(me, q.L.f.health) <= 0) return;
  const int32_t activator = q.ge(g_activator);
  if (!is_player(q, activator)) return;
  if (on(band(q.f(activator, q.L.f.flags), kFlNotarget))) return;
  q.set_i(me, q.L.f.enemy, activator);
  found_target(q);
}

// A piece thrown from `org`: it bounces, spins, passes through bodies and goes in 10 to 20 s. Its
// model's flag leaves the blood trail behind it.
void throw_gib_at(A& q, int32_t mdl, Vec3 org, float dm) {
  const int32_t g = q.spawn();
  q.set_i(g, q.L.f.classname, q.s(s_gib));
  q.set_f(g, q.L.f.movetype, kMovetypeBounce);
  q.set_f(g, q.L.f.solid, kSolidNot);
  q.set_v(g, q.L.f.velocity, gib_velocity(q, dm));
  q.set_v(g, q.L.f.avelocity, spin(q));
  q.setmodel(g, mdl);
  q.setsize(g, {0, 0, 0}, {0, 0, 0});
  q.setorigin(g, org);
  q.set_i(g, q.L.f.think, q.fn(fn_remove_self));
  const float t = q.time() + 10;
  q.set_f(g, q.L.f.nextthink, t + q.random() * 10);
}

// The body falls through its death frames and lies on the last: no longer solid after the kind's
// first few, a backpack dropped on the third by a kind that carries one.
void corpse_fall(A& q) {
  anim_tick(q);
  const int32_t me = q.self();
  if (q.f(me, f_anim_step) > q.f(me, f_death_solid)) q.set_f(me, q.L.f.solid, kSolidNot);
  if ((q.f(me, f_anim_step) == 3) & (q.f(me, f_drop_ammo) != 0)) {
    const float drop = q.f(me, f_drop_ammo);
    if (drop == kDropShells) q.set_f(me, q.L.f.ammo_shells, 5);
    if (drop == kDropCells) q.set_f(me, q.L.f.ammo_cells, 5);
    if (drop == kDropRockets) q.set_f(me, q.L.f.ammo_rockets, 2);
    backpack_drop(q);
  }
  if (q.f(q.self(), q.L.f.frame) >= q.v(q.self(), f_fr_now).y) return;
  set_think(q, q.self(), fn_corpse_fall, q.time() + 0.1f);
}

// Counted and told, its targets fired for its enemy, then the body: gibbed when the blow left it
// below the kind's gib health (the head left), else falling through a death sequence. The health
// goes no lower than -99, so the pieces fly at most twice as fast as a plain gibbing.
void monster_die(A& q) {
  q.add_to(q.L.g.killed_monsters, 1);
  q.WriteByte(kMsgAll, kSvcKilledmonster);
  q.set_ge(g_activator, q.i(q.self(), q.L.f.enemy));
  use_targets(q);
  const int32_t me = q.self();
  if (q.f(me, q.L.f.health) < -99) q.set_f(me, q.L.f.health, -99);
  q.set_f(me, q.L.f.deadflag, kDeadDead);
  q.set_f(me, q.L.f.takedamage, kDamageNo);
  const float flags = q.f(me, q.L.f.flags);
  q.set_f(me, q.L.f.flags, flags - band(flags, kFlMonster));
  set_think(q, me, fn_noop, -1);
  if (q.f(me, q.L.f.health) < q.f(me, f_gib_health)) {
    if (!q.is(q.i(me, f_gib_noise), "")) q.sound(me, kChanVoice, q.i(me, f_gib_noise), 1, kAttnNorm);
    else q.sound(me, kChanVoice, q.s(s_udeath_wav), 1, kAttnNorm);
    if (on(q.f(me, f_gib_first))) throw_pieces(q);
    become_head(q, q.i(me, f_head_model));
    if (q.f(me, f_gib_first) == 0) throw_pieces(q);
    return;
  }
  if (!q.is(q.i(me, q.L.f.noise1), "")) q.sound(me, kChanVoice, q.i(me, q.L.f.noise1), 1, kAttnNorm);
  if (q.v(me, f_fr_death).z == 0) {  // a kind with no death frames is laid on its side
    q.set_f(me, q.L.f.solid, kSolidNot);
    Vec3 a = q.v(me, q.L.f.angles);
    a.z = 90;
    q.set_v(me, q.L.f.angles, a);
    q.setorigin(me, q.v(me, q.L.f.origin) - Vec3{0, 0, 16});
    return;
  }
  if (q.f(me, f_death_solid) == 0) q.set_f(me, q.L.f.solid, kSolidNot);
  const bool two = q.v(me, f_fr_death2).z != 0;
  const bool half = q.random() < 0.5f;  // drawn either way
  if (two & half) anim_begin(q, q.v(me, f_fr_death2));
  else anim_begin(q, q.v(me, f_fr_death));
  corpse_fall(q);
}

// THE SPAWNING. Thinking begins: on a path when its target names a corner, else standing.
void monster_begin(A& q) {
  const int32_t me = q.self();
  q.set_f(me, f_pausetime, q.time() + kForever);
  if (!q.is(q.i(me, q.L.f.target), "")) {
    const int32_t corner = q.find(q.world(), q.L.f.targetname, q.i(me, q.L.f.target));
    if ((corner != 0) & q.is(q.i(corner, q.L.f.classname), "path_corner")) {
      q.set_i(me, f_movetarget, corner);
      q.set_i(me, q.L.f.goalentity, corner);
      q.set_f(me, q.L.f.ideal_yaw,
              q.vectoyaw(q.v(corner, q.L.f.origin) - q.v(me, q.L.f.origin)));
      q.set_f(me, f_pausetime, 0);
      run_field(q, me, f_th_walk);
      return;
    }
  }
  run_field(q, me, f_th_stand);
}

// What every kind's start shares: counted at once, solid, its model, box and health, the mind's
// hooks, thinking a random 0-0.5 s later.
void monster_start(A& q, int32_t mdl, float hp, Vec3 lo, Vec3 hi) {
  q.add_to(q.L.g.total_monsters, 1);
  const int32_t me = q.self();
  q.set_f(me, q.L.f.solid, kSolidSlidebox);
  q.set_f(me, q.L.f.movetype, kMovetypeStep);
  q.setmodel(me, mdl);
  q.setsize(me, lo, hi);
  q.set_f(me, q.L.f.health, hp);
  q.set_f(me, q.L.f.max_health, hp);
  q.set_f(me, q.L.f.takedamage, kDamageAim);
  q.set_f(me, q.L.f.flags, bor(q.f(me, q.L.f.flags), kFlMonster));
  q.set_v(me, q.L.f.view_ofs, {0, 0, 25});
  q.set_f(me, q.L.f.yaw_speed, 20);
  q.set_f(me, q.L.f.ideal_yaw, q.v(me, q.L.f.angles).y);
  q.set_i(me, q.L.f.use, q.fn(fn_monster_use));
  q.set_i(me, f_th_hurt, q.fn(fn_monster_hurt));
  q.set_i(me, f_th_stand, q.fn(fn_monster_stand));
  q.set_i(me, f_th_walk, q.fn(fn_monster_walk));
  q.set_i(me, f_th_run, q.fn(fn_monster_run));
  const float t = q.time() + 0.1f;
  set_think(q, me, fn_monster_begin, t + q.random() * 0.5f);
}

// A walker: raised a unit and dropped to the floor.
void walkmonster_start(A& q, int32_t mdl, float hp, Vec3 lo, Vec3 hi) {
  monster_start(q, mdl, hp, lo, hi);
  const int32_t me = q.self();
  q.setorigin(me, q.v(me, q.L.f.origin) + Vec3{0, 0, 1});
  if (q.droptofloor() == 0)
    q.dprint({q.i(me, q.L.f.classname), q.s(s_in_a_wall_at), q.vtos(q.v(me, q.L.f.origin)),
              q.s(s_nl)});
}

// A flyer: stays where it is placed, turns at half a walker's rate.
void flymonster_start(A& q, int32_t mdl, float hp, Vec3 lo, Vec3 hi) {
  monster_start(q, mdl, hp, lo, hi);
  const int32_t me = q.self();
  q.set_f(me, q.L.f.flags, bor(q.f(me, q.L.f.flags), kFlFly));
  q.set_f(me, q.L.f.yaw_speed, 10);
  q.setorigin(me, q.v(me, q.L.f.origin));
}

// A swimmer: stays where it is placed, in its water.
void swimmonster_start(A& q, int32_t mdl, float hp, Vec3 lo, Vec3 hi) {
  monster_start(q, mdl, hp, lo, hi);
  const int32_t me = q.self();
  q.set_f(me, q.L.f.flags, bor(q.f(me, q.L.f.flags), kFlSwim));
  q.set_f(me, q.L.f.yaw_speed, 10);
  q.setorigin(me, q.v(me, q.L.f.origin));
}

// In deathmatch the map's monsters are not there: true when this one was removed.
bool no_monsters_here(A& q) {
  if (q.deathmatch() == 0) return false;
  q.remove(q.self());
  return true;
}

// THE MISSILES.
void monster_missile_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  if (other == q.i(me, q.L.f.owner)) return;
  if (on(q.f(other, q.L.f.takedamage))) {
    q.set_i(other, f_deathtype, q.i(me, f_deathtype));
    damage(q, other, me, q.i(me, q.L.f.owner), q.f(me, f_dmg));
  }
  q.remove(q.self());
}

// The view vectors along `dir`: a direction's angles pitch up where a view's pitch down, so the
// pitch is turned over before the vectors are made.
void aim_along(A& q, Vec3 dir) {
  Vec3 a = q.vectoangles(dir);
  a.x = 0 - a.x;
  q.makevectors(a);
}

// A missile at the enemy's eye from the monster's: `mdl` at `spd` units a second, `points` on what
// it hits; `lob` units a second added upward (it then falls: a toss), else it flies straight.
int32_t monster_missile(A& q, int32_t mdl, float spd, float points, float lob) {
  const int32_t me = q.self();
  aim_along(q, eye(q, q.i(me, q.L.f.enemy)) - eye(q, me));
  int32_t m = 0;
  if (on(lob)) m = launch(q, mdl, kMovetypeToss, spd, 5);
  else m = launch(q, mdl, kMovetypeFlymissile, spd, 5);
  q.set_v(m, q.L.f.velocity, q.v(m, q.L.f.velocity) + fv(lob, {0, 0, 1}));
  q.set_i(m, q.L.f.classname, q.s(s_monster_missile));
  q.set_f(m, f_dmg, points);
  q.set_i(m, f_deathtype, q.s(s_hit));
  q.set_i(m, q.L.f.touch, q.fn(fn_monster_missile_touch));
  return m;
}

// A path corner: a monster walking to it goes on to the next; the last one stands it.
void corner_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  if (q.i(other, f_movetarget) != me) return;
  if (q.i(other, q.L.f.enemy) != 0) return;
  const int32_t next = q.find(q.world(), q.L.f.targetname, q.i(me, q.L.f.target));
  if (q.is(q.i(me, q.L.f.target), "") | (next == 0)) {
    q.set_i(other, f_movetarget, q.world());
    q.set_i(other, q.L.f.goalentity, q.world());
    q.set_f(other, f_pausetime, q.time() + kForever);
    q.set_self(other);
    run_field(q, other, f_th_stand);
    q.set_self(me);
    return;
  }
  q.set_i(other, f_movetarget, next);
  q.set_i(other, q.L.f.goalentity, next);
  q.set_f(other, q.L.f.ideal_yaw, q.vectoyaw(q.v(next, q.L.f.origin) - q.v(other, q.L.f.origin)));
}

void path_corner(A& q) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.solid, kSolidTrigger);
  q.set_i(me, q.L.f.touch, q.fn(fn_corner_touch));
  q.setsize(me, {-8, -8, -8}, {8, 8, 8});
}

// A flinch's step: `steps` 0.1 s steps of its frames, then running again.
void flinch(A& q, Fn again, float steps) {
  think_again(q, again);
  anim_tick(q);
  if (anim_step(q) >= steps) monster_run(q);
}

// What every kind sets the same way: its calls, its head, its gib health and its step distances.
void monster_kind(A& q, St sight, St death, St head, float gib_health, float walk, float run) {
  const int32_t me = q.self();
  q.set_i(me, q.L.f.noise, q.s(sight));
  q.set_i(me, q.L.f.noise1, q.s(death));
  q.set_i(me, f_head_model, q.s(head));
  q.set_f(me, f_gib_health, gib_health);
  q.set_f(me, f_walk_step, walk);
  q.set_f(me, f_run_step, run);
}

// Off the ground: the flag cleared.
void lands_no_flag(A& q, int32_t e) {
  const float flags = q.f(e, q.L.f.flags);
  q.set_f(e, q.L.f.flags, flags - band(flags, kFlOnground));
}

void ai_precache(A& q) {
  q.precache_model(q.s(s_gib1_mdl));
  q.precache_model(q.s(s_gib2_mdl));
  q.precache_model(q.s(s_gib3_mdl));
  q.precache_model(q.s(s_backpack_mdl));
  q.precache_sound(q.s(s_udeath_wav));
}

}  // namespace shooters_edge::server::rules::campaign
