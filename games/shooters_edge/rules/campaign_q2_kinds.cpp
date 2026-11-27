// Quake II's monsters in the classic campaign (classic remix's `cr_monsters q2`): each kind of
// campaign_q2.hpp spawned with the roster's health, box, speeds and attacks, Quake II's
// model, frames and sounds. They decide by Quake II's mind (server/q2_monster_mind.hpp, the same
// code Quake II's own rules decide by): whom they see, when they attack and with which attack,
// whether an attack goes on, the mutant's leap, whom they turn on when another monster hurts them,
// and the chase: with its foe out of sight it goes to where it last saw him, then follows the
// players' trail (8 marks the frame drops, `sq_trail_frame`), toward a goal of its own (`sq_goal`,
// a mark the campaign's step heads for round what blocks it). The campaign's states carry the rest
// (pain and death, campaign_ai.cpp). An
// attack is the roster's: a wind-up, shots on a loop `every` s apart, another loop at its odds
// while the enemy is in sight, then the recovery; the shots are the campaign's own kinds of harm
// (bolts, pellets, bullets, blows, grenades, rockets, a rail, a drain, a zap, a leap). A medic
// raises the dead it finds while it hunts; a power screen is armour that takes a third of each blow
// until it has taken 100 (from any side: the campaign's harm has no front).
//
// ge:summary Quake II's monsters run by the classic campaign: roster numbers, frames, attacks
// ge:keywords campaign cr_monsters remix q2-monsters monster-swap attack-loop roster
// ge:keywords monster_soldier monster_gunner monster_tank monster_gladiator
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#include <algorithm>
#include <cstdint>
#include <string_view>

#include "shooters_edge/rules/campaign_decl.hpp"
#include "shooters_edge/rules/campaign_q2.hpp"
#include "shooters_edge/server/q2_monster_mind.hpp"
#include "shooters_edge/server/q2_monster_poses.hpp"

namespace shooters_edge::server::rules::campaign {

namespace {

using roster::Fire;

int run_of(A& q, int32_t e) { return static_cast<int>(q.f(e, f_sq_kind)); }
const roster::Kind& kind_of(int run) {
  return roster::kKinds[sq_roster_of(kSqRuns[run].classname)];
}
int32_t word(A& q, int run, int w) { return q.str(kStringCount + run * kSqWords + w); }
int32_t shared(A& q, int w) { return q.str(kStringCount + kSqRunCount * kSqWords + w); }

// Attack `slot` of a kind: 0 and 1 its blows, 2 to 4 its ranged attacks.
const roster::Attack& attack_of(const roster::Kind& k, int slot) {
  return slot < 2 ? k.melee[slot] : k.missile[slot - 2];
}

Vec3 frames_of(SqSpan s) {
  if (s.none()) return {0, 0, 0};
  return {static_cast<float>(s.first), static_cast<float>(s.last), 1};
}

const SqMonsterPoses* poses_of(int run) {
  for (const SqMonsterPoses& p : kSqMonsterPoses)
    if (std::string_view(p.classname) == kSqRuns[run].classname) return &p;
  return nullptr;
}

Vec3 eye_of(A& q, int32_t e) { return q.v(e, q.L.f.origin) + q.v(e, q.L.f.view_ofs); }

int skill_of(A& q) { return static_cast<int>(q.cvar(q.s(s_skill))); }

// The campaign's random numbers as Quake II's mind draws them.
auto roll_of(A& q) {
  return [&q] { return static_cast<double>(q.random()); };
}

float distance_to(A& q, int32_t e) {
  return q.vlen(q.v(e, q.L.f.origin) - q.v(q.self(), q.L.f.origin));
}

// A clear line from its eye to `foe`'s: nothing solid between (monsters ignored), as Quake II's
// sight looks (water does not hide him).
bool clear_to(A& q, int32_t foe) {
  q.traceline(eye_of(q, q.self()), eye_of(q, foe), 1, q.self());
  return q.gf(q.L.g.trace_fraction) >= 1;
}

// A clear shot at `foe`: the line from its eye to his stopped by nothing, or by him.
bool shot_at(A& q, int32_t foe) {
  q.traceline(eye_of(q, q.self()), eye_of(q, foe), 0, q.self());
  return (q.gf(q.L.g.trace_fraction) >= 1) | (q.gi(q.L.g.trace_ent) == foe);
}

float points(A& q, const roster::Attack& a) {
  return static_cast<float>(a.damage) + q.random() * static_cast<float>(a.damage_max - a.damage);
}

// A loop of shots begins: the first after the wind-up, then one every `every` s until its end.
void begin_loop(A& q, const roster::Attack& a, bool first) {
  const int32_t me = q.self();
  float every = a.every, loop = a.loop;
  if (!first) {
    if (a.again_every > 0) every = a.again_every;
    if (a.again_loop > 0) loop = a.again_loop;
  }
  if (a.fire == Fire::burst && loop == 0) {
    loop = a.burst_min + q.random() * (a.burst_max - a.burst_min);
    every = 0.1f;
  } else if (a.fire == Fire::rockets && every == 0 && a.count > 1) {
    every = 0.2f;
    loop = 0.2f * static_cast<float>(a.count);
  } else if (loop == 0) {
    loop = 0.05f;  // one shot
  }
  const float start = first ? q.time() + a.windup : q.time();
  q.set_f(me, f_sq_next, start);
  q.set_f(me, f_sq_every, every > 0 ? every : loop + 1);
  q.set_f(me, f_sq_end, start + loop);
  q.set_f(me, f_sq_rest, 0);
}

// A missile of the kind's at the enemy, `dmg` on what it strikes.
int32_t missile(A& q, int model, float speed, float dmg, float lob) {
  return monster_missile(q, shared(q, model), speed, dmg, lob);
}

// A grenade at the enemy, lobbed, `dmg` on its blast 2.5 s on or on what it strikes.
int32_t grenade(A& q, float speed, float dmg) {
  const int32_t g = missile(q, kSqGrenade, speed, 0, 200);
  q.set_f(g, q.L.f.movetype, kMovetypeBounce);
  q.set_v(g, q.L.f.avelocity, {300, 300, 300});
  q.set_f(g, f_dmg, dmg);
  q.set_i(g, f_deathtype, q.s(s_blown_up_by_a_grenade));
  q.set_i(g, q.L.f.touch, q.fn(fn_sq_grenade_touch));
  q.set_i(g, q.L.f.think, q.fn(fn_sq_blast));
  q.set_f(g, q.L.f.nextthink, q.time() + 2.5f);
  return g;
}

void shoot(A& q, int run, const roster::Attack& a, bool blow) {
  const int32_t me = q.self();
  const int32_t enemy = q.i(me, q.L.f.enemy);
  const Vec3 from = eye_of(q, me);
  const Vec3 to = eye_of(q, enemy);
  q.sound(me, kChanWeapon, word(q, run, blow ? kSqBlow : kSqShot), 1, kAttnNorm);
  switch (a.fire) {
    case Fire::bolt:
    case Fire::sweep:
      for (int n = 0; n < a.count; n++) {
        const int32_t m = missile(q, kSqLaser, a.speed, points(q, a), 0);
        q.set_i(m, f_deathtype, q.s(s_lasered));
      }
      break;
    case Fire::pellets:
    case Fire::burst:
    case Fire::guns:
      aim_along(q, to - from);
      fire_pellets(q, a.fire == Fire::pellets ? static_cast<float>(a.count) : 1, points(q, a),
                   a.h / 8192);
      break;
    case Fire::grenade:
      grenade(q, a.speed, points(q, a));
      break;
    case Fire::rockets:
    case Fire::bfg: {
      const int32_t r = missile(q, kSqRocket, a.speed > 0 ? a.speed : 500, 0, 0);
      q.set_f(r, f_dmg, points(q, a));
      q.set_i(r, f_deathtype, q.s(s_blown_apart));
      q.set_i(r, q.L.f.touch, q.fn(fn_sq_rocket_touch));
      break;
    }
    case Fire::rail: {
      aim_along(q, to - from);
      q.traceline(from, from + q.v_forward() * 8192, 0, me);
      const Vec3 end = q.gv(q.L.g.trace_endpos);
      effect_beam(q, from, end);
      const int32_t hit = q.gi(q.L.g.trace_ent);
      if ((hit != 0) & on(q.f(hit, q.L.f.takedamage))) {
        q.set_i(hit, f_deathtype, q.s(s_shot));
        damage(q, hit, me, me, points(q, a));
      }
      break;
    }
    case Fire::drain:
    case Fire::zap:
    case Fire::tongue:  // a pack's pull and beams land as a blow where it sees him
    case Fire::laser:
      if ((a.fire == Fire::drain) & (q.vlen(to - from) > 256)) break;
      if (!can_see(q, enemy)) break;
      if (a.fire == Fire::drain) effect_beam(q, from, to);
      q.set_i(enemy, f_deathtype, q.s(s_mauled));
      damage(q, enemy, me, me, points(q, a));
      break;
    case Fire::club: {
      const Vec3 d = q.v(enemy, q.L.f.origin) - q.v(me, q.L.f.origin);
      if (q.vlen(d) > kind_of(run).maxs.x + 48) break;
      if (!can_see(q, enemy)) break;
      q.set_i(enemy, f_deathtype, q.s(s_mauled));
      damage(q, enemy, me, me, points(q, a));
      if (a.kick != 0)
        q.set_v(enemy, q.L.f.velocity,
                q.v(enemy, q.L.f.velocity) + q.normalize(d) * static_cast<float>(a.kick));
      break;
    }
    case Fire::leap:
      q.set_f(me, f_dmg, points(q, a));
      leap_off(q, a.speed, 250, fn_sq_leap_touch);
      break;
    case Fire::none:
      break;
  }
}

void start_attack(A& q, int slot) {
  const int32_t me = q.self();
  const int run = run_of(q, me);
  q.set_f(me, f_sq_slot, static_cast<float>(slot));
  q.set_f(me, f_sq_loops, 0);
  Vec3 frames = q.v(me, f_fr_run);
  if (const SqMonsterPoses* p = poses_of(run)) {
    const bool two = !p->attack[slot][1].none() && q.random() < 0.5f;
    if (!p->attack[slot][two ? 1 : 0].none()) frames = frames_of(p->attack[slot][two ? 1 : 0]);
  }
  anim_begin(q, frames);
  begin_loop(q, attack_of(kind_of(run), slot), true);
  sq_attack_step(q);
}

// THE DUCK: the kind's row (null for a kind that never ducks), and its box lowered or put back.
const roster::Duck* duck_row(int run) {
  for (const roster::Duck& d : roster::kDucks)
    if (std::string_view(d.classname) == kSqRuns[run].classname) return &d;
  return nullptr;
}

bool soldier(int run) {
  return std::string_view(kSqRuns[run].classname).starts_with("monster_soldier");
}

void duck_box(A& q, int run, bool low) {
  const roster::Kind& k = kind_of(run);
  const float drop = low ? q2_mind::kDuckDrop : 0;
  q.setsize(q.self(), {k.mins.x, k.mins.y, k.mins.z}, {k.maxs.x, k.maxs.y, k.maxs.z - drop});
}

// The duck over: the box back if it is still low, and the run again next think.
void duck_done(A& q, int run) {
  const int32_t me = q.self();
  if (on(q.f(me, f_sq_ducked))) duck_box(q, run, false);
  q.set_f(me, f_sq_duck_frame, 0);
  q.set_f(me, f_sq_ducked, 0);
  q.set_f(me, f_sq_duck_still, 0);
  think_again(q, fn_monster_run);
}

// A shot offered to the monster (self) from player `shooter`, `dist` along its line at `speed`:
// from in front, one in four ducked, a soldier's held past its arrival and at times shot from.
void dodge(A& q, int32_t shooter, float dist, float speed) {
  const int32_t me = q.self();
  const int run = run_of(q, me);
  if ((duck_row(run) == nullptr) | (q.f(me, q.L.f.health) <= 0) | (q.f(me, q.L.f.deadflag) != 0))
    return;
  const Vec3 to = q.normalize(q.v(shooter, q.L.f.origin) - q.v(me, q.L.f.origin));
  q.makevectors({0, q.v(me, q.L.f.angles).y, 0});
  const Vec3 f = q.v_forward();
  if (to.x * f.x + to.y * f.y + to.z * f.z <= q2_mind::kInFront) return;
  const double eta = q2_mind::dodge_eta(dist, kind_of(run).maxs.x, speed);
  if (!q2_mind::dodges(roll_of(q))) return;
  if (q.i(me, q.L.f.enemy) == 0) {  // found silently: no sight call, no shot on the run
    const int32_t call = q.i(me, q.L.f.noise);
    q.set_i(me, q.L.f.enemy, shooter);
    q.set_i(me, q.L.f.noise, q.s(s_empty));
    found_target(q);
    q.set_self(me);
    q.set_i(me, q.L.f.noise, call);
  }
  const q2_mind::DuckPlan p =
      q2_mind::duck_plan(soldier(run), kSqRuns[run].classname == std::string_view("monster_brain"),
                         skill_of(q), q.time(), eta, roll_of(q));
  if (p.hold >= 0) q.set_f(me, f_sq_duck_hold, static_cast<float>(p.hold));
  if ((q.f(me, f_sq_duck_frame) > 0) & (on(q.f(me, f_sq_duck_shoot)) == p.shoots))
    return;                         // goes on
  q.set_f(me, f_sq_duck_frame, 1);  // its first frame (frames are kept one up: 0 is none)
  q.set_f(me, f_sq_duck_still, 0);
  q.set_f(me, f_sq_duck_shoot, p.shoots ? 1 : 0);
  q.set_i(me, q.L.f.think, q.fn(fn_sq_duck));  // an attack or a flinch cut short
  q.set_f(me, q.L.f.nextthink, q.time());
}

// THE TRAIL: 8 marks, chained by `sq_goal` from the global `sq_trail`; a mark's place, its time
// (`sq_trail_time`) and the way he went (its yaw). Made with the first of Quake II's monsters.
int32_t mark(A& q, int i) {
  int32_t m = q.ge(g_sq_trail);
  for (int n = 0; n < i && m != 0; n++) m = q.i(m, f_sq_goal);
  return m;
}

int32_t new_mark(A& q) {
  const int32_t m = q.spawn();
  q.set_i(m, q.L.f.classname, q.s(s_empty));
  q.set_f(m, q.L.f.solid, kSolidNot);
  q.set_f(m, q.L.f.movetype, kMovetypeNone);
  q.setsize(m, {0, 0, 0}, {0, 0, 0});
  return m;
}

void make_trail(A& q) {
  if (q.ge(g_sq_trail) != 0) return;
  int32_t next = 0;
  for (int i = 0; i < q2_mind::kTrailMarks; i++) {
    const int32_t m = new_mark(q);
    q.set_i(m, f_sq_goal, next);
    next = m;
  }
  q.set_ge(g_sq_trail, next);
  q.set_g(g_sq_trail_head, 0);
}

// The chase's state as Quake II's mind reads it, from the monster's fields and back.
struct ChaseState {
  bool lost = false, on_trail = false, side_step = false;
  Vec3 goal{}, real_goal{};
  double search_time = 0, trail_time = 0;
};

ChaseState chase_of(A& q, int32_t me) {
  ChaseState c;
  c.lost = on(q.f(me, f_sq_lost));
  c.on_trail = on(q.f(me, f_sq_on_trail));
  c.side_step = on(q.f(me, f_sq_side_step));
  c.real_goal = q.v(me, f_sq_real_goal);
  const int32_t g = q.i(me, f_sq_goal);
  c.goal = g != 0 ? q.v(g, q.L.f.origin) : q.v(me, f_sq_last_sight);
  c.search_time = q.f(me, f_sq_search);
  c.trail_time = q.f(me, f_sq_trail_time);
  return c;
}

// The kind's spawn: its numbers, model, frames and sounds, over the campaign's minds.
void spawn(A& q, int run) {
  if (no_monsters_here(q)) return;
  const int32_t self = q.self();
  make_trail(q);
  q.set_self(self);
  const roster::Kind& k = kind_of(run);
  for (int w = kSqSight; w < kSqWords; w++) q.precache_sound(word(q, run, w));
  for (int w = kSqLaser; w <= kSqHead; w++) q.precache_model(shared(q, w));
  q.precache_model(word(q, run, kSqModel));
  const int32_t me = q.self();
  q.set_f(me, f_sq_kind, static_cast<float>(run));
  q.set_i(me, q.L.f.noise, word(q, run, kSqSight));
  q.set_i(me, q.L.f.noise1, word(q, run, kSqDeath));
  q.set_i(me, f_head_model, shared(q, kSqHead));
  q.set_f(me, f_gib_health, k.gib == roster::kNoGib ? -100000 : static_cast<float>(k.gib));
  q.set_f(me, f_walk_step, k.walk * 0.1f);
  q.set_f(me, f_run_step, k.run * 0.1f);
  q.set_f(me, q.L.f.skin, static_cast<float>(k.skin));
  q.set_i(me, f_th_check, q.fn(k.medic ? fn_sq_medic_check : fn_sq_check));
  if (k.screen) {
    q.set_f(me, q.L.f.armortype, 1.0f / 3.0f);
    q.set_f(me, q.L.f.armorvalue, 100);
  }
  if (k.melee[0].fire != Fire::none) q.set_i(me, f_th_melee, q.fn(fn_sq_melee));
  if (k.missile[0].fire != Fire::none) q.set_i(me, f_th_missile, q.fn(fn_sq_missile));
  q.set_i(me, f_th_pain, q.fn(fn_sq_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  if (const SqMonsterPoses* p = poses_of(run)) {
    q.set_v(me, f_fr_stand, frames_of(p->stand));
    q.set_v(me, f_fr_walk, frames_of(p->walk.none() ? p->run : p->walk));
    q.set_v(me, f_fr_run, frames_of(p->run.none() ? p->walk : p->run));
    q.set_v(me, f_fr_pain, frames_of(p->pain[0]));
    q.set_v(me, f_fr_death, frames_of(p->death[0]));
    q.set_v(me, f_fr_death2, frames_of(p->death[1]));
  }
  q.set_f(me, f_death_solid, 2);
  const Vec3 lo{k.mins.x, k.mins.y, k.mins.z}, hi{k.maxs.x, k.maxs.y, k.maxs.z};
  const auto hp = static_cast<float>(k.health);
  if (k.flies)
    flymonster_start(q, word(q, run, kSqModel), hp, lo, hi);
  else if (k.swims)
    swimmonster_start(q, word(q, run, kSqModel), hp, lo, hi);
  else
    walkmonster_start(q, word(q, run, kSqModel), hp, lo, hi);
}

// A body a medic may raise: a monster's corpse lying still (its fall over), not torn apart.
bool raisable(A& q, int32_t e) {
  return (q.f(e, q.L.f.deadflag) == kDeadDead) & (q.f(e, q.L.f.movetype) == kMovetypeStep) &
         (q.i(e, f_th_run) != 0) & (q.f(e, q.L.f.frame) >= q.v(e, f_fr_now).y);
}

// The dead monster `c` up again: its health whole, solid, counted, and after the medic's enemy.
void raise(A& q, int32_t c) {
  const int32_t me = q.self();
  q.set_f(c, q.L.f.health, q.f(c, q.L.f.max_health));
  q.set_f(c, q.L.f.deadflag, 0);
  q.set_f(c, q.L.f.takedamage, kDamageAim);
  q.set_f(c, q.L.f.solid, kSolidSlidebox);
  q.set_f(c, q.L.f.flags, bor(q.f(c, q.L.f.flags), kFlMonster));
  q.set_f(c, q.L.f.frame, q.v(c, f_fr_stand).x);
  q.setorigin(c, q.v(c, q.L.f.origin));
  q.add_to(q.L.g.total_monsters, 1);
  q.set_i(c, q.L.f.enemy, q.i(me, q.L.f.enemy));
  q.set_f(c, f_sq_duck_frame, 0);  // up again out of no duck and no shot on the run
  q.set_f(c, f_sq_ducked, 0);
  q.set_f(c, f_sq_run_step, 0);
  q.set_i(c, q.L.f.think, q.fn(fn_monster_run));
  q.set_f(c, q.L.f.nextthink, q.time() + 0.1f);
}

}  // namespace

// A medic's choice while it hunts: a corpse it can see within 400 units first (raised within 96,
// walked to before that), else the usual attack.
void sq_medic_check(A& q) {
  const int32_t me = q.self();
  const Vec3 at = q.v(me, q.L.f.origin);
  int32_t best = 0;
  float nearest = 400;
  for (int32_t e = q.nextent(q.world()); e != 0; e = q.nextent(e)) {  // corpses are not solid
    if ((e == me) || !raisable(q, e)) continue;
    const float d = q.vlen(q.v(e, q.L.f.origin) - at);
    if (d >= nearest) continue;
    q.traceline(eye_of(q, me), q.v(e, q.L.f.origin), 1, me);
    if (q.gf(q.L.g.trace_fraction) < 1) continue;
    best = e;
    nearest = d;
  }
  if (best == 0) {
    q.ret_f(sq_choose(q) ? 1.0f : 0.0f);
    return;
  }
  const float yaw = q.vectoyaw(q.v(best, q.L.f.origin) - at);
  q.set_f(me, q.L.f.ideal_yaw, yaw);
  q.changeyaw();
  if (nearest > 96) {
    q.walkmove(yaw, q.f(me, f_run_step));
  } else {
    q.sound(me, kChanWeapon, word(q, run_of(q, me), kSqBlow), 1, kAttnNorm);
    effect_beam(q, eye_of(q, me), q.v(best, q.L.f.origin));
    raise(q, best);
  }
  q.ret_f(1);
}

// A blow: the second at even odds where the kind has two (Quake II's pick).
void sq_melee(A& q) {
  start_attack(q, q2_mind::pick_blow(kind_of(run_of(q, q.self())), roll_of(q)));
}

// A ranged attack by Quake II's pick (the first nearer than the kind's `close`, the tank's by
// range, else by weight within each attack's reach).
void sq_missile(A& q) {
  start_attack(q,
               q2_mind::pick_ranged(kind_of(run_of(q, q.self())), enemy_distance(q), roll_of(q)));
}

// THE DECISION TO ATTACK, Quake II's (q2_monster_mind.hpp): a clear shot, facing him within 45
// degrees, then at melee range a blow (or its shot, for a kind with none), else by the chance a
// frame of its range and the skill once its wait is spent; the attack starts at once.
bool sq_choose(A& q) {
  const int32_t me = q.self();
  const int32_t enemy = q.i(me, q.L.f.enemy);
  if (!shot_at(q, enemy)) return false;
  const Vec3 to = q.v(enemy, q.L.f.origin) - q.v(me, q.L.f.origin);
  if (!q2_mind::faces(q.v(me, q.L.f.angles).y, q.vectoyaw(to))) return false;  // the run turns it
  const roster::Kind& k = kind_of(run_of(q, me));
  const auto roll = roll_of(q);
  const int skill = skill_of(q);
  const float dist = q.vlen(to);
  const q2_mind::Range r = q2_mind::range_of(dist);
  const bool has_missile = k.missile[0].fire != Fire::none;
  const bool has_melee = k.melee[0].fire != Fire::none;
  if (k.test == roster::Test::mutant) {
    if (r == q2_mind::Range::melee) {
      start_attack(q, 0);
      return true;
    }
    const float z = q.v(me, q.L.f.origin).z;
    const float bottom = q.v(enemy, q.L.f.origin).z + q.v(enemy, q.L.f.mins).z;
    const float height = q.v(enemy, q.L.f.maxs).z - q.v(enemy, q.L.f.mins).z;
    if (!q2_mind::leap_test(z + k.mins.z, z + k.maxs.z, bottom, height, q.vlen({to.x, to.y, 0}),
                            roll))
      return false;
    start_attack(q, 2);
    return true;
  }
  if (r == q2_mind::Range::melee) {
    if (q2_mind::blow_skipped(k.test, skill, roll)) return false;
    if (has_melee) {
      start_attack(q, q2_mind::pick_blow(k, roll));
      return true;
    }
    if (has_missile) {  // no blow: the ranged attack, always
      const int i = q2_mind::pick_ranged(k, dist, roll);
      if (dist < attack_of(k, i).closest) return false;
      start_attack(q, i);
      return true;
    }
  }
  if (!has_missile || r == q2_mind::Range::far || q.time() < q.f(me, f_attack_finished))
    return false;
  if (roll() >= q2_mind::attack_chance(k.test, false, r, skill)) return false;
  const int i = q2_mind::pick_ranged(k, dist, roll);
  q.set_f(me, f_attack_finished, q.time() + static_cast<float>(q2_mind::kRefire * roll()));
  if (dist < attack_of(k, i).closest) return false;  // too near for it: keeps closing
  start_attack(q, i);
  return true;
}

void sq_check(A& q) { q.ret_f(sq_choose(q) ? 1.0f : 0.0f); }

// Whether entity `e` is one of Quake II's monsters the campaign runs.
bool sq_runs(A& q, int32_t e) { return (e != 0) && q.i(e, f_th_pain) == q.fn(fn_sq_pain); }

// Quake II's sight of `foe`: never far or through anything solid; at melee range whichever way it
// faces; near, the foe it hunts or a player who fired in the last second; else only in front.
bool sq_sees(A& q, int32_t foe) {
  const int32_t me = q.self();
  q2_mind::Sighting look;
  Vec3 to = q.v(foe, q.L.f.origin) - q.v(me, q.L.f.origin);
  look.range = q2_mind::range_of(q.vlen(to));
  if (look.range == q2_mind::Range::far) return false;
  look.clear = clear_to(q, foe);
  if (!look.clear) return false;
  look.hunted = foe == q.i(me, q.L.f.enemy);
  look.fired = q.f(foe, f_show_hostile) >= q.time();
  to.z = 0;
  const float across = q.vlen(to);
  if (across == 0) {
    look.ahead = true;
  } else {
    q.makevectors(Vec3{0, q.v(me, q.L.f.angles).y, 0});
    const Vec3 f = q.v_forward();
    look.ahead = q2_mind::in_front(to.x / across, to.y / across, f.x, f.y);
  }
  return q2_mind::sees(look);
}

// Hurt by monster `a`: Quake II's rule of whom it turns on (one unlike it that moves as it does,
// not a heavy kind, or one aiming at it; else it joins `a` against `a`'s foe).
bool sq_hurt_by_monster(A& q, int32_t a) {
  const int32_t me = q.self();
  const auto flags = [&](int32_t e, int f) { return on(band(q.f(e, q.L.f.flags), float(f))); };
  q2_mind::Attacker by;
  by.alike = flags(a, kFlFly) == flags(me, kFlFly) && flags(a, kFlSwim) == flags(me, kFlSwim);
  by.same_kind = q.same(q.i(a, q.L.f.classname), q.i(me, q.L.f.classname));
  for (std::string_view h : q2_mind::kHeavy)
    by.heavy = by.heavy || q.is(q.i(a, q.L.f.classname), h);
  const int32_t his = q.i(a, q.L.f.enemy);
  by.aiming_at_me = his == me;
  by.has_live_foe = (his != 0) && (q.f(his, q.L.f.health) > 0);
  const q2_mind::Turn t = q2_mind::infight(by);
  const int32_t foe = t == q2_mind::Turn::attacker        ? a
                      : t == q2_mind::Turn::attackers_foe ? his
                                                          : 0;
  if ((foe == 0) || (foe == q.i(me, q.L.f.enemy)) || (foe == me)) return false;
  if (q.is(q.i(q.i(me, q.L.f.enemy), q.L.f.classname), "player"))
    q.set_i(me, f_oldenemy, q.i(me, q.L.f.enemy));
  q.set_i(me, q.L.f.enemy, foe);
  found_target(q);
  return true;
}

// A step of an attack: the shots that fall in it, then another loop or the recovery.
void sq_attack_step(A& q) {
  think_again(q, fn_sq_attack_step);
  const int32_t me = q.self();
  const int run = run_of(q, me);
  const int slot = static_cast<int>(q.f(me, f_sq_slot));
  const roster::Attack& a = attack_of(kind_of(run), slot);
  frame_loop(q, q.v(me, f_fr_now));
  const float rest = q.f(me, f_sq_rest);
  if (rest > 0) {
    if (q.time() >= rest) monster_run(q);
    return;
  }
  if (a.fire != Fire::leap) face_enemy(q);
  if (slot < 2) ai_charge(q, q.f(me, f_run_step) * 0.3f);
  const int32_t enemy = q.i(me, q.L.f.enemy);
  while ((q.time() + 0.001f >= q.f(me, f_sq_next)) & (q.f(me, f_sq_next) < q.f(me, f_sq_end)) &
         (q.f(enemy, q.L.f.health) > 0)) {
    shoot(q, run, a, slot < 2);
    q.set_f(me, f_sq_next, q.f(me, f_sq_next) + q.f(me, f_sq_every));
  }
  if (q.time() + 0.001f < q.f(me, f_sq_end)) return;
  const float loops = q.f(me, f_sq_loops);
  q2_mind::Loop l;
  l.loops = static_cast<int>(loops);
  l.skill = skill_of(q);
  l.foe_alive = q.f(enemy, q.L.f.health) > 0;
  l.blow = slot < 2;
  l.range = q2_mind::range_of(distance_to(q, enemy));
  l.mutant = kind_of(run).test == roster::Test::mutant;
  l.clear = l.foe_alive && !l.blow && shot_at(q, enemy);
  const bool more = q2_mind::goes_on(a, l, roll_of(q));
  if (more) {
    q.set_f(me, f_sq_loops, loops + 1);
    begin_loop(q, a, false);
    return;
  }
  q.set_f(me, f_sq_rest, q.time() + (a.recover > 0.1f ? a.recover : 0.1f));
}

// Pain: its cry and its flinch, no more than every 3 s (th_pain, the kind's mark; a blow takes
// the table's way, sq_take_pain).
void sq_pain(A& q) {
  const int32_t me = q.self();
  if (on(q.f(me, f_sq_ducked))) return;  // ducked: no pain
  hold_pain(q, static_cast<float>(q2_mind::kPainEvery));
  q.sound(me, kChanVoice, word(q, run_of(q, me), kSqPain), 1, kAttnNorm);
  if (q.f(me, f_sq_duck_frame) > 0) return;  // going down: the duck goes on
  anim_begin(q, q.v(me, f_fr_pain));
  sq_flinch(q);
}

// A blow it lives through, as Quake II's pain table has it (q2_monster_pain_rules.hpp): by the
// health it took (from what it had after the last), the skill (a kind that flinches at skill 3
// reads it as 2), how fast it rises, the frame it shows and whether that is a pain's. Its wait is
// 3 s from a pain spent; while it lasts only a pain run may be swapped for the knocked-up one. At
// skill 3 every blow puts the next pain 5 s off. Ducked, nothing; going down, the cry alone.
void sq_take_pain(A& q) {
  const int32_t me = q.self();
  const int run = run_of(q, me);
  const roster::Kind& k = kind_of(run);
  const float health = q.f(me, q.L.f.health);
  float had = q.f(me, f_sq_health);
  if (had < health) had = static_cast<float>(k.health);  // new (none kept yet), or raised
  q.set_f(me, f_sq_health, health);
  if (on(q.f(me, f_sq_ducked))) return;  // ducked: no pain
  const int skill = skill_of(q);
  const bool waiting = q.time() < q.f(me, f_pain_finished);
  const bool in_pain = q.i(me, q.L.f.think) == q.fn(fn_sq_flinch);
  const PainChoice c =
      q2_pain::pain(kSqRuns[run].classname, had > health ? static_cast<int>(had - health) : 0,
                    k.pain_at_3 ? std::min(skill, 2) : skill, q.v(me, q.L.f.velocity).z,
                    static_cast<int>(q.f(me, q.L.f.frame)), static_cast<int>(q.v(me, f_fr_now).x),
                    in_pain, waiting, roll_of(q));
  if (c.spent) q.set_f(me, f_pain_finished, q.time() + static_cast<float>(q2_mind::kPainEvery));
  if (c.sound != nullptr)
    for (const SqWord w : {kSqPain, kSqCry1, kSqCry2})
      if (std::string_view(sq_word(run, w)) == c.sound) {
        q.sound(me, kChanVoice, word(q, run, w), 1, c.everywhere ? kAttnNone : kAttnNorm);
        break;
      }
  if (!c.run.none() && !(q.f(me, f_sq_duck_frame) > 0)) {  // the flinch cuts what it was doing
    q.set_f(me, f_sq_run_step, 0);
    anim_begin(q, frames_of(c.run));
    sq_flinch(q);
  }
  if (skill == 3)
    q.set_f(me, f_pain_finished,
            q.time() + 2 + static_cast<float>(q2_mind::kPainEvery));  // the next 5 s off
}

void sq_flinch(A& q) {
  const Vec3 f = q.v(q.self(), f_fr_now);
  flinch(q, fn_sq_flinch, f.z == 0 ? 3 : f.y - f.x + 1);
}

// A grenade's or a rocket's blast: its points less half the distance to everything near.
void sq_blast(A& q) {
  const int32_t me = q.self();
  radius_damage(q, me, q.i(me, q.L.f.owner), q.f(me, f_dmg), q.world(), q.i(me, f_deathtype));
  q.sound(me, kChanVoice, q.s(s_weapons_r_exp3_wav), 1, kAttnNorm);
  effect_explosion(q, q.v(me, q.L.f.origin));
  q.remove(me);
}

void sq_grenade_touch(A& q) {
  if (q.other() == q.i(q.self(), q.L.f.owner)) return;
  if (q.f(q.other(), q.L.f.takedamage) == kDamageAim) {
    sq_blast(q);
    return;
  }
  q.sound(q.self(), kChanVoice, q.s(s_weapons_bounce_wav), 1, kAttnNorm);
}

void sq_rocket_touch(A& q) {
  if (q.other() == q.i(q.self(), q.L.f.owner)) return;
  sq_blast(q);
}

// A leap strikes what it lands on at speed, once.
void sq_leap_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  if (on(q.f(other, q.L.f.takedamage)) && q.vlen(q.v(me, q.L.f.velocity)) > 400) {
    q.set_i(other, f_deathtype, q.s(s_mauled));
    damage(q, other, me, me, q.f(me, f_dmg));
  }
  q.set_i(me, q.L.f.touch, 0);
}

// Found a foe: the chase starts from where he is now.
void sq_found(A& q) {
  const int32_t me = q.self();
  q.set_v(me, f_sq_last_sight, q.v(q.i(me, q.L.f.enemy), q.L.f.origin));
  q.set_f(me, f_sq_lost, 0);
  q.set_f(me, f_sq_on_trail, 0);
  q.set_f(me, f_sq_search, 0);
  q.set_f(me, f_sq_trail_time, q.time());
  q.set_f(me, f_sq_sound, 0);
  // A soldier finding him 500 or more off, above the easiest skill, fires on the run half the
  // time; not when found silently (by a dodge).
  const int run = run_of(q, me);
  if (q.is(q.i(me, q.L.f.noise), "")) return;
  if (q2_mind::runs_and_shoots(soldier(run), skill_of(q), distance_to(q, q.i(me, q.L.f.enemy)),
                               roll_of(q)))
    q.set_f(me, f_sq_run_step, 1);  // its first step (kept one up: 0 is none)
}

// HEARING, Quake II's: with no enemy, a player's shot of the last fifth of a second within 1000
// units that it may hear (in its sight's set of places; an ambusher must see where it was) turns it
// to the noise and sends it there, its first shot a second off; so does where his shot landed in
// that fifth, within 1000 and in a clear line from it (no ambusher). True when it heard one now or
// goes to one already.
bool sq_hear(A& q) {
  const int32_t me = q.self();
  if (q.i(me, q.L.f.enemy) != 0) return false;
  const bool ambush = band(q.f(me, q.L.f.spawnflags), kSpawnAmbush) != 0;
  const int32_t near = ambush ? 0 : q.checkclient();
  for (int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player)); p != 0;
       p = q.find(p, q.L.f.classname, q.s(s_player))) {
    if ((q.f(p, q.L.f.health) <= 0) || on(band(q.f(p, q.L.f.flags), kFlNotarget))) continue;
    if (q.f(p, f_show_hostile) < q.time() + 0.8f) continue;  // no shot this fifth of a second
    const Vec3 at = q.v(p, q.L.f.origin);
    if (q.vlen(at - q.v(me, q.L.f.origin)) > q2_mind::kHear) continue;
    if (ambush) {
      q.traceline(eye_of(q, me), at, 1, me);
      if (q.gf(q.L.g.trace_fraction) < 1) continue;
    } else if (near != p) {
      continue;
    }
    q.set_f(me, f_sq_sound, q.time());
    q.set_v(me, f_sq_sound_at, at);
    q.set_f(me, q.L.f.ideal_yaw, q.vectoyaw(at - q.v(me, q.L.f.origin)));
    q.set_f(me, f_attack_finished, q.time() + static_cast<float>(q2_mind::kFirstWait));
    return true;
  }
  // Where his shot landed, within 1000 and in a clear line from it: heard by one with no enemy
  // that lies in no ambush.
  if (q2_mind::hears_impact(ambush, false)) {
    for (int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player)); p != 0;
         p = q.find(p, q.L.f.classname, q.s(s_player))) {
      if ((q.f(p, q.L.f.health) <= 0) || on(band(q.f(p, q.L.f.flags), kFlNotarget))) continue;
      const float when = q.f(p, f_sq_impact);
      if (when <= 0 || when < q.time() - static_cast<float>(q2_mind::kHeardFor)) continue;
      const Vec3 at = q.v(p, f_sq_impact_at);
      if (q.vlen(at - q.v(me, q.L.f.origin)) > q2_mind::kHear) continue;
      q.traceline(at, eye_of(q, me), 1, me);
      if (q.gf(q.L.g.trace_fraction) < 1) continue;
      q.set_f(me, f_sq_sound, q.time());
      q.set_v(me, f_sq_sound_at, at);
      q.set_f(me, q.L.f.ideal_yaw, q.vectoyaw(at - q.v(me, q.L.f.origin)));
      q.set_f(me, f_attack_finished, q.time() + static_cast<float>(q2_mind::kFirstWait));
      return true;
    }
  }
  return q.f(me, f_sq_sound) > 0;
}

// A player's shot landed at `at` (`by` the player or his missile): Quake II's monsters may hear it.
void sq_note_impact(A& q, int32_t by, Vec3 at) {
  const int32_t p = q.is(q.i(by, q.L.f.classname), "player") ? by : q.i(by, q.L.f.owner);
  if (p == 0 || !q.is(q.i(p, q.L.f.classname), "player")) return;
  if (on(band(q.f(p, q.L.f.flags), kFlNotarget))) return;
  q.set_f(p, f_sq_impact, q.time());
  q.set_v(p, f_sq_impact_at, at);
}

// A step toward the noise it heard: within 64 units it stands facing it, and 5 s on forgets it
// (back to its path, or to stand). False when it heard none.
bool sq_to_sound(A& q) {
  const int32_t me = q.self();
  const float heard = q.f(me, f_sq_sound);
  if (heard <= 0) return false;
  const Vec3 to = q.v(me, f_sq_sound_at) - q.v(me, q.L.f.origin);
  if (q.vlen({to.x, to.y, 0}) <= q2_mind::kSoundNear) {
    q.set_f(me, q.L.f.ideal_yaw, q.vectoyaw(to));
    q.changeyaw();
    if (q.time() <= heard + static_cast<float>(q2_mind::kSoundForget)) return true;
    q.set_f(me, f_sq_sound, 0);
    if (q.i(me, f_movetarget) != 0) {
      q.set_i(me, q.L.f.goalentity, q.i(me, f_movetarget));
      return false;
    }
    q.set_f(me, f_pausetime, q.time() + kForever);
    run_field(q, me, f_th_stand);
    return true;
  }
  int32_t g = q.i(me, f_sq_goal);
  if (g == 0) {
    g = new_mark(q);
    q.set_i(me, f_sq_goal, g);
  }
  q.setorigin(g, q.v(me, f_sq_sound_at));
  q.set_i(me, q.L.f.goalentity, g);
  q.movetogoal(q.f(me, f_run_step));
  return true;
}

// A frame of the hunt: in a clear line it goes at him (and keeps where it saw him); out of sight,
// Quake II's chase (q2_monster_mind.hpp's `pursue`) sets its goal: the last sighting, the trail's
// marks after its last look, one move at him 20 s on. The campaign's step makes for the goal.
void sq_chase(A& q) {
  const int32_t me = q.self();
  const int32_t enemy = q.i(me, q.L.f.enemy);
  if (clear_to(q, enemy)) {
    q.set_v(me, f_sq_last_sight, q.v(enemy, q.L.f.origin));
    q.set_f(me, f_sq_trail_time, q.time());
    q.set_f(me, f_sq_lost, 0);
    q.set_f(me, f_sq_on_trail, 0);
    q.set_i(me, q.L.f.goalentity, enemy);
    return;
  }
  ChaseState c = chase_of(q, me);
  struct World {
    A& q;
    int32_t me, enemy;
    double now() const { return q.time(); }
    Vec3 last_sight() const { return q.v(me, f_sq_last_sight); }
    Vec3 foe_at() const { return q.v(enemy, q.L.f.origin); }
    bool far_from(Vec3 goal) const {
      const Vec3 d = goal - q.v(me, q.L.f.origin);
      return q.vlen({d.x, d.y, 0}) > q.f(me, f_run_step) + 1;
    }
    int next_mark(bool first) const {
      double times[q2_mind::kTrailMarks];
      for (int i = 0; i < q2_mind::kTrailMarks; i++) times[i] = q.f(mark(q, i), f_sq_trail_time);
      return q2_mind::next_mark(times, static_cast<int>(q.g(g_sq_trail_head)),
                                q.f(me, f_sq_trail_time), first, [&](int i) {
                                  q.traceline(eye_of(q, me), q.v(mark(q, i), q.L.f.origin), 1, me);
                                  return q.gf(q.L.g.trace_fraction) >= 1;
                                });
    }
    Vec3 mark_at(int i) const { return q.v(mark(q, i), q.L.f.origin); }
    double mark_time(int i) const { return q.f(mark(q, i), f_sq_trail_time); }
    float mark_yaw(int i) const { return q.v(mark(q, i), q.L.f.angles).y; }
    void face(float yaw) const {
      Vec3 a = q.v(me, q.L.f.angles);
      a.y = yaw;
      q.set_v(me, q.L.f.angles, a);
      q.set_f(me, q.L.f.ideal_yaw, yaw);
    }
    // A side step round what blocks the new goal (q2_mind::side_point), its box swept against
    // the world and every body but its own; the view axes kept round it.
    ChaseState& c;
    float sweep(Vec3 to) const {
      q.tracebox(q.v(me, q.L.f.origin), q.v(me, q.L.f.mins), q.v(me, q.L.f.maxs), to, 0, me);
      return q.gf(q.L.g.trace_fraction);
    }
    float length(Vec3 v) const { return q.vlen(v); }
    void axes(Vec3 dir, Vec3& forward, Vec3& right) const {
      q.makevectors({0, q.vectoyaw(dir), 0});
      forward = q.v_forward();
      right = q.gv(q.L.g.v_right);
    }
    void correct() const {
      const Vec3 fwd = q.v_forward(), rt = q.gv(q.L.g.v_right), up = q.gv(q.L.g.v_up);
      const Vec3 at = q.v(me, q.L.f.origin);
      Vec3 side{};
      const bool stepped = q2_mind::side_point(at, c.goal, *this, side);
      q.set_gv(q.L.g.v_forward, fwd);
      q.set_gv(q.L.g.v_right, rt);
      q.set_gv(q.L.g.v_up, up);
      if (!stepped) return;
      c.real_goal = c.goal;
      c.side_step = true;
      c.goal = side;
      face(q.vectoyaw(side - at));
    }
  } w{q, me, enemy, c};
  q2_mind::pursue(c, w);
  q.set_f(me, f_sq_side_step, c.side_step ? 1.0f : 0.0f);
  q.set_v(me, f_sq_real_goal, c.real_goal);
  q.set_f(me, f_sq_lost, c.lost ? 1.0f : 0.0f);
  q.set_f(me, f_sq_on_trail, c.on_trail ? 1.0f : 0.0f);
  q.set_f(me, f_sq_search, static_cast<float>(c.search_time));
  q.set_f(me, f_sq_trail_time, static_cast<float>(c.trail_time));
  int32_t g = q.i(me, f_sq_goal);
  if (g == 0) {
    g = new_mark(q);
    q.set_i(me, f_sq_goal, g);
  }
  q.setorigin(g, c.goal);
  q.set_i(me, q.L.f.goalentity, g);
}

// The players' trail, each frame while Quake II's monsters are about: a mark where a player stood
// last frame once his eye no longer sees the newest mark.
void sq_trail_frame(A& q) {
  if (q.ge(g_sq_trail) == 0) return;
  for (int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player)); p != 0;
       p = q.find(p, q.L.f.classname, q.s(s_player))) {
    if (q.f(p, q.L.f.health) <= 0) continue;
    const Vec3 at = q.v(p, q.L.f.origin);
    const Vec3 last = q.v(p, f_sq_trail_last);
    q.set_v(p, f_sq_trail_last, at);
    if ((last.x == 0) & (last.y == 0) & (last.z == 0)) continue;  // new: no mark yet
    const int head = static_cast<int>(q.g(g_sq_trail_head));
    const int32_t newest = mark(q, q2_mind::newest_mark(head));
    q.traceline(eye_of(q, p), q.v(newest, q.L.f.origin), 1, p);
    if (q.gf(q.L.g.trace_fraction) >= 1) continue;
    const int32_t m = mark(q, head);
    q.setorigin(m, last);
    q.set_f(m, f_sq_trail_time, q.time());
    q.set_v(m, q.L.f.angles, {0, q.vectoyaw(last - q.v(newest, q.L.f.origin)), 0});
    q.set_g(g_sq_trail_head, static_cast<float>((head + 1) % q2_mind::kTrailMarks));
  }
}

// DODGING: a player's bolt, rocket or nail just launched (`missile`, self its shooter) offered to
// the first of Quake II's monsters on its line, as Quake II's dodge offers it (an easiest skill's
// one in four). Nothing is drawn on a map with none of them. The firer's view axes are kept.
void sq_offer_dodge(A& q, int32_t missile) {
  const int32_t shooter = q.self();
  if ((q.ge(g_sq_trail) == 0) | on(q.deathmatch()) | !q.is(q.i(shooter, q.L.f.classname), "player"))
    return;
  if (!q2_mind::dodge_offered(skill_of(q), roll_of(q))) return;
  const Vec3 fwd = q.v_forward(), right = q.gv(q.L.g.v_right), up = q.gv(q.L.g.v_up);
  const Vec3 v = q.v(missile, q.L.f.velocity);
  const Vec3 from = eye_of(q, shooter);
  q.traceline(from, from + q.normalize(v) * 8192, 0, shooter);
  const int32_t hit = q.gi(q.L.g.trace_ent);
  if ((hit != 0) && sq_runs(q, hit)) {
    const float dist = q.vlen(q.gv(q.L.g.trace_endpos) - q.v(missile, q.L.f.origin));
    q.set_self(hit);
    dodge(q, shooter, dist, q.vlen(v));
    q.set_self(shooter);
  }
  q.set_gv(q.L.g.v_forward, fwd);
  q.set_gv(q.L.g.v_right, right);
  q.set_gv(q.L.g.v_up, up);
}

// One 0.1 s frame of a dodge's duck: the frame shown and, new, its step, the box down (held low a
// second, the brain's hold its own) and the duck that shoots firing (a gunner above skill 1 throws
// a grenade ahead half the time), the box up; then the next frame, the same while held, or the run.
void sq_duck(A& q) {
  think_again(q, fn_sq_duck);
  const int32_t me = q.self();
  const int run = run_of(q, me);
  const bool shoots = on(q.f(me, f_sq_duck_shoot));
  const roster::Duck* d = shoots ? &roster::kDuckShoot : duck_row(run);
  const int f = static_cast<int>(q.f(me, f_sq_duck_frame)) - 1;
  if ((d == nullptr) | (f < 0)) {
    duck_done(q, run);
    return;
  }
  q.set_f(me, q.L.f.frame, static_cast<float>(d->first + f));
  const int32_t enemy = q.i(me, q.L.f.enemy);
  const bool foe = (enemy != 0) && (q.f(enemy, q.L.f.health) > 0);
  if (shoots && foe) face_enemy(q);
  if (!on(q.f(me, f_sq_duck_still))) {
    const float walk = static_cast<float>(d->steps[f]) * d->scale;
    if (walk != 0) q.walkmove(q.v(me, q.L.f.angles).y, walk);
    if (f == d->down) {
      const bool was = on(q.f(me, f_sq_ducked));
      if (!was) {
        q.set_f(me, f_sq_ducked, 1);
        duck_box(q, run, true);
        if (kSqRuns[run].classname != std::string_view("monster_brain"))
          q.set_f(me, f_sq_duck_hold, q.time() + static_cast<float>(q2_mind::kDuckLow));
      }
      if (shoots && (enemy != 0)) shoot(q, run, attack_of(kind_of(run), 2), false);
      if (!was && (kSqRuns[run].classname == std::string_view("monster_gunner")) &&
          (skill_of(q) >= 2) && (q.random() > 0.5f)) {  // a grenade straight ahead, unaimed
        const int32_t g = grenade(q, 600, 50);
        q.makevectors({0, q.v(me, q.L.f.angles).y, 0});
        q.set_v(g, q.L.f.velocity, q.v_forward() * 600 + Vec3{0, 0, 200});
      }
    }
    if ((f == d->up) && on(q.f(me, f_sq_ducked))) {
      q.set_f(me, f_sq_ducked, 0);
      duck_box(q, run, false);
    }
  }
  const int next = q2_mind::duck_next(*d, f, shoots, q.time(), q.f(me, f_sq_duck_hold));
  q.set_f(me, f_sq_duck_still, next == f ? 1 : 0);  // held low: shown again, still
  if (next < 0) {
    duck_done(q, run);
    return;
  }
  q.set_f(me, f_sq_duck_frame, static_cast<float>(next + 1));
}

// One 0.1 s frame of a soldier's shot on the run: at him facing him, the shot at its fourth step,
// round again at skill 3 while he is 500 or more off; then the run.
void sq_run_shoot(A& q) {
  const int32_t me = q.self();
  const int run = run_of(q, me);
  const int at = static_cast<int>(q.f(me, f_sq_run_step)) - 1;
  const int32_t enemy = q.i(me, q.L.f.enemy);
  if ((at < 0) | (enemy == 0) || (q.f(enemy, q.L.f.health) <= 0)) {
    q.set_f(me, f_sq_run_step, 0);
    think_again(q, fn_monster_run);
    return;
  }
  think_again(q, fn_sq_run_shoot);
  q.set_f(me, q.L.f.frame, static_cast<float>(q2_mind::kRunShootFirst + at));
  ai_charge(q, q.f(me, f_run_step));
  if (at == q2_mind::kRunShootFires) shoot(q, run, attack_of(kind_of(run), 2), false);
  const int next =
      q2_mind::run_shoot_next(at, skill_of(q), q.f(enemy, q.L.f.health) > 0, distance_to(q, enemy));
  q.set_f(me, f_sq_run_step, static_cast<float>(next + 1));
  if (next < 0) think_again(q, fn_monster_run);
}

// What goes before Quake II's monster's run: a duck it is in, or its shot on the run. True when one
// took the frame.
bool sq_busy(A& q) {
  const int32_t me = q.self();
  if (q.f(me, f_sq_duck_frame) > 0) {
    sq_duck(q);
    return true;
  }
  if (q.f(me, f_sq_run_step) > 0) {
    sq_run_shoot(q);
    return true;
  }
  return false;
}

#define SE_SQ_SPAWN(name, i)                                      \
  static_assert(std::string_view(kSqRuns[i].classname) == #name); \
  void name(A& q) { spawn(q, i); }
SE_SQ_SPAWN(monster_soldier_light, 0)
SE_SQ_SPAWN(monster_soldier, 1)
SE_SQ_SPAWN(monster_soldier_ss, 2)
SE_SQ_SPAWN(monster_infantry, 3)
SE_SQ_SPAWN(monster_gunner, 4)
SE_SQ_SPAWN(monster_berserk, 5)
SE_SQ_SPAWN(monster_gladiator, 6)
SE_SQ_SPAWN(monster_flyer, 7)
SE_SQ_SPAWN(monster_parasite, 8)
SE_SQ_SPAWN(monster_mutant, 9)
SE_SQ_SPAWN(monster_chick, 10)
SE_SQ_SPAWN(monster_tank, 11)
SE_SQ_SPAWN(monster_flipper, 12)
SE_SQ_SPAWN(monster_floater, 13)
SE_SQ_SPAWN(monster_medic, 14)
SE_SQ_SPAWN(monster_brain, 15)
SE_SQ_SPAWN(monster_hover, 16)
SE_SQ_SPAWN(monster_tank_commander, 17)
#undef SE_SQ_SPAWN

}  // namespace shooters_edge::server::rules::campaign
