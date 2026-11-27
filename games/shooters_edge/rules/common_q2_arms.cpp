// QUAKE II'S WEAPONS FIRED BY THE CLASSIC RULES (classic remix's `cr_items q2`): a slot whose row
// fires `q2_*` is Quake II's weapon whole, stepped by Quake II's own hand and shots
// (plugins/classic/q2_arms.hpp) under the classic rules' armour, quad and scoring.
//
// - **The hand** steps ten times a second, as Quake II's server does: a weapon taken up runs its
//   raise, a ready one its idle, attack fires from the first fire frame and a shot goes on each
//   fire frame, so each weapon keeps its own period (the shotgun 1.2 s, the railgun 1.6 s) and the
//   held-fire runs (the machinegun, the chaingun's spin up to three bullets a frame, the
//   hyperblaster's loop). The frame is the player's `weaponframe`, so the view model plays it.
// - **The shots**: the blaster's and hyperblaster's bolts, the shotgun's 12 pellets and the super
//   shotgun's two halves of 10 turned 5 degrees each way, the bullets' spread, the chaingun's
//   muzzle, the grenade's throw and fuse, the rocket at 650 with its hit of 100 to 120 and its
//   blast, the rail through every body in its line; each from Quake II's muzzle offset, its
//   damage (deathmatch's in deathmatch) and its knockback (500 x kick / 200; 1600 x on oneself).
// - **What they make**: Quake II's samples (each shot's, the pump and reload after it, the
//   chaingun's spin, the click when empty), the hits' and blasts' effects by Quake II's names
//   (puffs, sparks, blood, the rail's line, the blasts and their water kinds), and its obituaries.
// - **The footsteps** (`cr_sounds q2`, which sets `g_footsteps`): each player's stride on Quake
//   II's 0.1 s clock, a step at each stride it turns over, as `g_footsteps` says when.
//
// ge:summary The classic rules firing Quake II's weapons whole: its hand, shots, hits and sounds
// ge:keywords remix cr_items q2-arms real-weapons hand-step railgun shotgun-period pellets
// ge:keywords bullets chaingun hyperblaster bolt rocket grenade q2-knockback q2-obituary
// ge:keywords q2_footsteps footsteps g_footsteps cr_sounds
// ge:skill progs shooter
// ge:test tests/shooters_edge_remix_q2_arms_test.cpp
#include <cstdint>
#include <string_view>

#include "classic/q2_arms.hpp"
#include "classic/q2_view.hpp"
#include "core/num.hpp"
#include "shooters_edge/rules/q2_arm_strings.hpp"
#include "shooters_edge/rules/variant.hpp"
#if defined(SE_RULES_CAMPAIGN)
#include "shooters_edge/rules/campaign_q2.hpp"
#endif

namespace shooters_edge::server::rules::SE_RULES_NS {

namespace {

namespace gc = ge::classic;

#if defined(SE_RULES_CAMPAIGN)
constexpr int kQ2Base = kStringCount + kSqStringCount;
#else
constexpr int kQ2Base = kStringCount;
#endif

constexpr float kStep = 0.1f;  // the hand's step, s (Quake II's server frame)
constexpr float kMostSteps = 4;
enum HandState : uint8_t { kReady = 0, kRaising = 1, kFiring = 2 };

// One of Quake II's weapons' strings, by its index (q2_arm_strings.hpp).
int32_t q2s(A& q, int index) { return q.str(kQ2Base + index); }
template <size_t N>
constexpr int idx(const char (&name)[N]) {
  return q2_arm_string(std::string_view(name, N - 1));
}
constexpr int idx(const char* name) { return q2_arm_string(name); }

// The Quake II weapon a held weapon `w` is (its row's fire word), else -1.
int arm_of(A& q, float w) {
  const int32_t fire = weapon_word(q, w, 0);
  for (int a = 0; a < gc::kQ2ArmCount; a++)
    if (q.is(fire, gc::kQ2ArmFireWords[a])) return a;
  return -1;
}

const gc::Q2Arm& arm(int a) { return gc::q2_arm(static_cast<gc::Q2ArmId>(a)); }
bool in_deathmatch(A& q) { return q.deathmatch() != 0; }
int hit_points(A& q, int a) { return in_deathmatch(q) ? arm(a).damage_dm : arm(a).damage_sp; }
bool quad_on(A& q, int32_t e) { return q.f(e, f_super_damage_finished) > q.time(); }

// A sample now, or `delay` s later from a helper (the pump and reload after the shot).
void sound_at(A& q, int32_t e, float chan, int index, float delay) {
  if (index < 0) return;
  if (delay <= 0) {
    q.sound(e, chan, q2s(q, index), 1, kAttnNorm);
    return;
  }
  const int32_t h = q.spawn();
  q.set_i(h, q.L.f.owner, e);
  q.set_f(h, f_q2_sound, static_cast<float>(index));
  q.set_f(h, q.L.f.nextthink, q.time() + delay);
  q.set_i(h, q.L.f.think, q.fn(fn_q2_sound_later));
}

// An effect by Quake II's name at a point, facing `dir`.
void effect_at(A& q, int index, Vec3 at, Vec3 dir) {
  q.pointparticles(q.particleeffectnum(q2s(q, index)), at, dir, 1);
}

// Quake II's knockback for the blows that follow: `kick` (or, below 0, each blow's own points),
// four times under the quad.
void blow_kick(A& q, float kick) {
  q.set_g(g_q2_blow, 1);
  q.set_g(g_q2_kick, kick);
}
void blow_done(A& q) {
  q.set_g(g_q2_blow, 0);
  q.set_g(g_q2_kick, 0);
}

// What a kill by Quake II's weapon `a` reads (`blast`: by its splash).
void kill_words(A& q, int32_t targ, int a, bool blast) {
  const gc::Q2ArmWords w = gc::q2_arm_words(static_cast<gc::Q2ArmId>(a), blast);
  if (w.words != nullptr) q.set_i(targ, f_deathtype, q2s(q, idx(w.words)));
}

// A blow of `points` to `targ` by `me`'s weapon `a`, Quake II's knockback `kick`.
void hit(A& q, int32_t targ, int32_t inflictor, int32_t attacker, float points, float kick, int a,
         bool blast) {
  if (q.f(targ, q.L.f.takedamage) == 0) return;
  kill_words(q, targ, a, blast);
  blow_kick(q, quad_on(q, attacker) ? kick * 4 : kick);
  damage(q, targ, inflictor, attacker, points);
  blow_done(q);
}

// A blast at `inflictor` of `points` over `radius`: less half the distance to each thing's
// middle, half on the attacker, the knockback the blow's points; `spare` is left out.
void q2_radius(A& q, int32_t inflictor, int32_t attacker, float points, int32_t spare, float radius,
               int a) {
  const Vec3 at = q.v(inflictor, q.L.f.origin);
  int32_t e = q.findradius(at, radius);
  while (e != 0) {
    const int32_t next = q.i(e, q.L.f.chain);
    if ((e != spare) & (q.f(e, q.L.f.takedamage) != 0)) {
      const Vec3 mid = q.v(e, q.L.f.origin) + (q.v(e, q.L.f.mins) + q.v(e, q.L.f.maxs)) * 0.5f;
      float d = points - 0.5f * q.vlen(at - mid);
      if (e == attacker) d = d * 0.5f;
      if ((d > 0) & quake_sees(q, inflictor, e)) {
        kill_words(q, e, a, true);
        blow_kick(q, -1);
        damage(q, e, inflictor, attacker, d);
        blow_done(q);
      }
    }
    e = next;
  }
}

Vec3 eye(A& q, int32_t e) { return q.v(e, q.L.f.origin) + q.v(e, q.L.f.view_ofs); }

// Quake II's muzzle: `forward` and `right` along the view from the origin, the eye less 8 up.
Vec3 muzzle(A& q, int32_t e, float forward, float right) {
  const Vec3 o = q.v(e, q.L.f.origin);
  return Vec3{o.x, o.y, o.z + q.v(e, q.L.f.view_ofs).z - 8} + q.v_forward() * forward +
         q.v_right() * right;
}

// A line shot from `start` along `dir`, spread `h` across and `v` up at its reach: a blow on
// what it hits, else a puff (`puff`) where it lands, never on the sky.
void line_shot(A& q, Vec3 start, Vec3 dir, Vec3 right, Vec3 up, float h, float v, float points,
               float kick, int a, int puff) {
  const int32_t me = q.self();
  // Nothing shoots from inside a wall: the eye to the muzzle first.
  q.traceline(eye(q, me), start, 0, me);
  if (q.gf(q.L.g.trace_fraction) < 1) start = q.gv(q.L.g.trace_endpos) - dir;
  const float r = crandom(q) * h, u = crandom(q) * v;
  const Vec3 end = start + dir * gc::kQ2ShotRange + right * r + up * u;
  q.traceline(start, end, 0, me);
  if (q.gf(q.L.g.trace_fraction) >= 1) return;
  const int32_t ent = q.gi(q.L.g.trace_ent);
  const Vec3 at = q.gv(q.L.g.trace_endpos);
  const Vec3 normal = q.gv(q.L.g.trace_plane_normal);
  if (q.f(ent, q.L.f.takedamage) != 0) {
    effect_at(q, idx(gc::kQ2HitBlood), at, normal);
    hit(q, ent, me, me, points, kick, a, false);
    return;
  }
  if (q.pointcontents(at + dir * 2) == kContentSky) return;
#if defined(SE_RULES_CAMPAIGN)
  sq_note_impact(q, me, at);  // Quake II's monsters may hear where it landed
#endif
  effect_at(q, puff, at, normal);
}

void pellets(A& q, Vec3 dir, Vec3 right, Vec3 up, int count, float h, float v, int a, float kick) {
  const int32_t me = q.self();
  const Vec3 start = muzzle(q, me, arm(a).forward, arm(a).right);
  for (int i = 0; i < count; i++)
    line_shot(q, start, dir, right, up, h, v, static_cast<float>(hit_points(q, a)), kick, a,
              idx(gc::kQ2HitPellet));
}

// A missile of row `w` from the muzzle along the view.
int32_t missile(A& q, float w, int a, float mtype, float spd, float life) {
  const int32_t me = q.self();
  const int32_t m = q.spawn();
  q.set_i(m, q.L.f.owner, me);
  q.set_f(m, q.L.f.movetype, mtype);
  q.set_f(m, q.L.f.solid, kSolidBbox);
  q.set_v(m, q.L.f.velocity, q.v_forward() * spd);
  q.set_v(m, q.L.f.angles, q.vectoangles(q.v_forward()));
  q.set_f(m, q.L.f.nextthink, q.time() + life);
  q.set_i(m, q.L.f.think, q.fn(fn_remove_self));
  q.setmodel(m, weapon_missile(q, w));
  q.setsize(m, {0, 0, 0}, {0, 0, 0});
  Vec3 start = muzzle(q, me, arm(a).forward, arm(a).right);
  q.traceline(eye(q, me), start, 0, me);
  if (q.gf(q.L.g.trace_fraction) < 1) start = q.gv(q.L.g.trace_endpos) - q.v_forward();
  q.setorigin(m, start);
  q.set_f(m, q.L.f.weapon, w);
  q.set_f(m, f_q2_arm, static_cast<float>(a));
  missile_now(q, m);
#if defined(SE_RULES_CAMPAIGN)
  if (mtype == kMovetypeFlymissile) sq_offer_dodge(q, m);  // a bolt or a rocket: the dodge
#endif
  return m;
}

void fire_bolt(A& q, float w, int a) {
  const int32_t m = missile(q, w, a, kMovetypeFlymissile, gc::kQ2BoltSpeed, gc::kQ2BoltLife);
  q.set_i(m, q.L.f.classname, q2s(q, idx("bolt")));  // drawn as Quake II's bolt (its streak)
  q.set_f(m, q.L.f.effects, kEfDimlight);
  q.set_f(m, f_dmg, static_cast<float>(hit_points(q, a)));
  q.set_i(m, q.L.f.touch, q.fn(fn_q2_bolt_touch));
}

void fire_grenade(A& q, float w, int a) {
  const int32_t m = missile(q, w, a, kMovetypeBounce, gc::kQ2GrenadeSpeed, gc::kQ2GrenadeFuse);
  const float r1 = crandom(q), r2 = crandom(q);
  const Vec3 v = q.v_forward() * gc::kQ2GrenadeSpeed +
                 q.v_up() * (gc::kQ2GrenadeLift + r1 * gc::kQ2GrenadeScatter) +
                 q.v_right() * (r2 * gc::kQ2GrenadeScatter);
  q.set_v(m, q.L.f.velocity, v);
  q.set_v(m, q.L.f.avelocity, {300, 300, 300});
  q.set_i(m, q.L.f.classname, q.s(s_grenade));
  q.set_f(m, f_dmg, static_cast<float>(hit_points(q, a)));
  q.set_i(m, q.L.f.touch, q.fn(fn_q2_grenade_touch));
  q.set_i(m, q.L.f.think, q.fn(fn_q2_grenade_blast));
}

void fire_rocket(A& q, float w, int a) {
  const int32_t m = missile(q, w, a, kMovetypeFlymissile, gc::kQ2RocketSpeed, gc::kQ2RocketLife);
  q.set_i(m, q.L.f.classname, q.s(s_rocket));
  q.set_f(m, f_dmg, static_cast<float>(hit_points(q, a)) + q.random() * gc::kQ2RocketExtra);
  q.set_i(m, q.L.f.touch, q.fn(fn_q2_rocket_touch));
  midair_fired(q, m);
}

// The rail: through every body in its line to the first wall, each hit once, the line drawn.
void fire_rail(A& q, int a) {
  const int32_t me = q.self();
  Vec3 start = muzzle(q, me, arm(a).forward, arm(a).right);
  q.traceline(eye(q, me), start, 0, me);
  if (q.gf(q.L.g.trace_fraction) < 1) start = q.gv(q.L.g.trace_endpos) - q.v_forward();
  const Vec3 end = start + q.v_forward() * gc::kQ2ShotRange;
  const float kick = in_deathmatch(q) ? gc::kQ2RailKickDm : gc::kQ2RailKickSp;
  Vec3 from = start;
  int32_t ignore = me;
  Vec3 last = end;
  for (int through = 0; through < 16; through++) {
    q.traceline(from, end, 0, ignore);
    last = q.gv(q.L.g.trace_endpos);
    if (q.gf(q.L.g.trace_fraction) >= 1) break;
    const int32_t ent = q.gi(q.L.g.trace_ent);
    if (q.f(ent, q.L.f.takedamage) == 0 || ent == q.world()) break;
    effect_at(q, idx(gc::kQ2HitBlood), last, q.gv(q.L.g.trace_plane_normal));
    hit(q, ent, me, me, static_cast<float>(hit_points(q, a)), kick, a, false);
    ignore = ent;
    from = last;
  }
  q.trailparticles(me, q.particleeffectnum(q2s(q, idx(gc::kQ2RailLine))), start, last);
}

// The draw of a rest on an idle pause frame: off the rules' tenth and the player, so a rest moves
// none of the rules' own random draws.
uint32_t pause_draw(A& q, int32_t me) {
  const auto tenth =
      static_cast<uint32_t>(static_cast<int64_t>(ge::core::num::round(q.time() * 10.0f)));
  return gc::q2_pause_draw(tenth, static_cast<uint32_t>(me));
}

// One step's `shots` of weapon `a` (row `w`), the `held`th shot in a row. The machinegun's bullets
// go where its kick tips the view: its climb (outside deathmatch) and its yaw and roll shake, the
// shake drawn as the window draws its own (`gc::q2_shot_kick`), off the tenth and the player.
void fire(A& q, float w, int a, int shots, int held) {
  const int32_t me = q.self();
  Vec3 aim = q.v(me, q.L.f.v_angle);
  if (static_cast<gc::Q2ArmId>(a) == gc::Q2ArmId::machinegun) {
    const float none[3] = {0, 0, 0};
    const gc::Q2Kick k =
        gc::q2_shot_kick(gc::Q2ArmId::machinegun, none,
                         gc::q2_machine_climb(held - 1, in_deathmatch(q)), pause_draw(q, me));
    aim = {aim.x + k.angles[0], aim.y + k.angles[1], aim.z + k.angles[2]};
  }
  q.makevectors(aim);
  const Vec3 dir = q.v_forward(), right = q.v_right(), up = q.v_up();
  const float kick = static_cast<float>(arm(a).kick);
  switch (static_cast<gc::Q2ArmId>(a)) {
    case gc::Q2ArmId::blaster:
    case gc::Q2ArmId::hyperblaster:
      fire_bolt(q, w, a);
      break;
    case gc::Q2ArmId::shotgun:
      pellets(q, dir, right, up, gc::kQ2ShotgunPellets, gc::kQ2ShotgunSpreadH,
              gc::kQ2ShotgunSpreadV, a, kick);
      break;
    case gc::Q2ArmId::super_shotgun: {
      const Vec3 view = q.v(me, q.L.f.v_angle);
      for (const float turn : {-gc::kQ2SuperTurn, gc::kQ2SuperTurn}) {
        q.makevectors({view.x, view.y + turn, view.z});
        pellets(q, q.v_forward(), q.v_right(), q.v_up(), gc::kQ2SuperHalfPellets,
                gc::kQ2SuperSpreadH, gc::kQ2SuperSpreadV, a, kick);
      }
      q.makevectors(view);
      break;
    }
    case gc::Q2ArmId::machinegun:
    case gc::Q2ArmId::chaingun:
      for (int i = 0; i < shots; i++) {
        float across = arm(a).right, rise = 0;
        if (static_cast<gc::Q2ArmId>(a) == gc::Q2ArmId::chaingun) {
          across = gc::kQ2ChainAcross + crandom(q) * gc::kQ2ChainJitter;
          rise = crandom(q) * gc::kQ2ChainJitter;
        }
        const Vec3 start = muzzle(q, me, arm(a).forward, across) + up * rise;
        line_shot(q, start, dir, right, up, gc::kQ2BulletSpreadH, gc::kQ2BulletSpreadV,
                  static_cast<float>(hit_points(q, a)), kick, a, idx(gc::kQ2HitBullet));
      }
      break;
    case gc::Q2ArmId::grenade_launcher:
      fire_grenade(q, w, a);
      break;
    case gc::Q2ArmId::rocket_launcher:
      fire_rocket(q, w, a);
      break;
    case gc::Q2ArmId::railgun:
      fire_rail(q, a);
      break;
    default:  // the BFG and the hand grenade have no slot in the classic rules
      break;
  }
  // The samples: the shot's (the bullets' drawn among five), then the action worked after it.
  gc::Q2ArmSound heard[3] = {};
  const int n = gc::q2_arm_shot_sounds(static_cast<gc::Q2ArmId>(a), shots, heard);
  for (int k = 0; k < n; k++) {
    int index = idx(heard[k].name);
    if (heard[k].variants > 0) {
      const int pick = static_cast<int>(q.random() * static_cast<float>(heard[k].variants));
      index = idx(gc::kQ2GunSounds[pick < heard[k].variants ? pick : 0]);
    }
    sound_at(q, me, k == 0 ? kChanWeapon : kChanAuto, index, heard[k].delay);
  }
}

bool can_fire(A& q, float w, int a) {
  return arm(a).per_shot == 0 || ammo_for(q, w) >= static_cast<float>(arm(a).per_shot);
}

// One step of the hand of weapon `w` (Quake II's `a`).
void hand_step(A& q, float w, int a) {
  const int32_t me = q.self();
  const gc::Q2Arm& r = arm(a);
  int frame = static_cast<int>(q.f(me, f_q2_frame));
  int held = static_cast<int>(q.f(me, f_q2_held));
  const int was = frame;
  const bool was_firing = q.f(me, f_q2_state) == static_cast<float>(kFiring);
  const bool attack = q.f(me, q.L.f.button0) != 0;
  switch (static_cast<int>(q.f(me, f_q2_state))) {
    case kRaising:
      if (gc::q2_raise_step(r.frames, frame)) q.set_f(me, f_q2_state, kReady);
      break;
    case kReady:
      if (!attack) {
        gc::q2_idle_step(r.frames, gc::kQ2ArmPauses[a], frame, pause_draw(q, me));
        break;
      }
      if (!can_fire(q, w, a)) {
        if (q.time() >= q.f(me, f_q2_click) + 1) {
          sound_at(q, me, kChanAuto, idx(gc::kQ2NoAmmo), 0);
          q.set_f(me, f_q2_click, q.time());
        }
        weapon_select_best(q);
        return;
      }
      q.set_f(me, f_q2_state, kFiring);  // from the first fire frame, in this same step
      frame = r.frames.last_activate + 1;
      held = 0;
      [[fallthrough]];
    case kFiring: {
      const float stock = ammo_for(q, w);
      const int have = r.per_shot == 0 ? 1 << 20 : static_cast<int>(stock) / r.per_shot;
      const bool again = attack && can_fire(q, w, a);
      const gc::Q2Firing f = gc::q2_firing_step(r.frames, r.held, frame, held, attack, have, again);
      if (f.done) {
        q.set_f(me, f_q2_state, kReady);
        break;
      }
      if (f.shots > 0) {
        if (r.per_shot > 0) ammo_spend(q, w, static_cast<float>(f.shots * r.per_shot));
        q.set_f(me, f_show_hostile, q.time() + 1);
        q.set_f(me, q.L.f.effects, bor(q.f(me, q.L.f.effects), kEfMuzzleflash));
        if (quake_rules(q)) quake_quad_heard(q);
        fire(q, w, a, f.shots, held);
      }
      break;
    }
    default:
      break;
  }
  const bool firing = q.f(me, f_q2_state) == static_cast<float>(kFiring);
  if (firing) q.set_f(me, f_attack_finished, q.time() + kStep);
  const int sound = gc::q2_arm_sound(static_cast<gc::Q2ArmId>(a), was, was_firing, frame, firing);
  if (sound >= 0) sound_at(q, me, kChanAuto, idx(gc::kQ2ArmSounds[sound]), 0);
  q.set_f(me, f_q2_frame, static_cast<float>(frame));
  q.set_f(me, f_q2_held, static_cast<float>(held));
  q.set_f(me, q.L.f.weaponframe, static_cast<float>(frame));
}

}  // namespace

bool q2_weapon_frame(A& q) {
  if (!on(q.g(g_q2_arms))) return false;
  const int32_t me = q.self();
  const float w = q.f(me, q.L.f.weapon);
  const int a = arm_of(q, w);
  if (a < 0) {
    q.set_f(me, f_q2_arm, 0);
    return false;
  }
  if (q.f(me, f_q2_arm) != static_cast<float>(a + 1)) {  // taken up: the raise from frame 0
    q.set_f(me, f_q2_arm, static_cast<float>(a + 1));
    q.set_f(me, f_q2_state, kRaising);
    q.set_f(me, f_q2_frame, 0);
    q.set_f(me, f_q2_held, 0);
    q.set_f(me, f_q2_next, q.time());
    q.set_f(me, q.L.f.weaponframe, 0);
  }
  if (q.f(me, q.L.f.deadflag) != kDeadNo) return true;
  if (q.f(me, f_q2_next) < q.time() - kStep * kMostSteps) q.set_f(me, f_q2_next, q.time());
  while (q.time() >= q.f(me, f_q2_next)) {
    q.set_f(me, f_q2_next, q.f(me, f_q2_next) + kStep);
    hand_step(q, w, a);
    if (arm_of(q, q.f(me, q.L.f.weapon)) != a) break;  // the click switched weapons
  }
  return true;
}

bool q2_arsenal(A& q) {
  for (int slot = 1; slot <= 8; slot++)
    if (arm_of(q, weapon_of_rank(static_cast<float>(slot))) >= 0) return true;
  return false;
}

void q2_weapon_precache(A& q) {
  // Quake II's footsteps (classic remix's `cr_sounds q2` sets `g_footsteps`), read once.
  q.set_g(g_q2_steps, q.cvar(q2s(q, idx("g_footsteps"))));
  if (q.g(g_q2_steps) >= 1)
    for (const char* s : gc::kQ2Footsteps) q.precache_sound(q2s(q, idx(s)));
  q.set_g(g_q2_arms, q2_arsenal(q) ? 1.0f : 0.0f);  // read once: the frames and deaths ask it
  if (!on(q.g(g_q2_arms))) return;
  for (int i = 0; i < kQ2ArmStringCount; i++) {
    const std::string_view s = kQ2ArmStrings[static_cast<size_t>(i)];
    if (s.ends_with(".wav")) q.precache_sound(q2s(q, i));
  }
}

// Quake II's footsteps on the classic rules, under `g_footsteps` (classic remix's `cr_sounds q2`):
// the stride's clock stepped on Quake II's 0.1 s frames (plugins/classic/q2_view.hpp), a step's
// sound on the body at each stride it turns over, one of the four at random.
void q2_footsteps(A& q) {
  const float when = q.g(g_q2_steps);
  if (when < 1) return;
  const int32_t me = q.self();
  if (q.f(me, f_q2_stride_next) > q.time() + 1) q.set_f(me, f_q2_stride_next, 0);  // a new map
  const Vec3 v = q.v(me, q.L.f.velocity);
  const float speed = q.vlen({v.x, v.y, 0});
  const bool ground = band(q.f(me, q.L.f.flags), kFlOnground) != 0;
  for (int n = 0; n < 4 && q.f(me, f_q2_stride_next) <= q.time(); n++) {
    q.set_f(me, f_q2_stride_next,
            q.f(me, f_q2_stride_next) <= 0 ? q.time() + kStep : q.f(me, f_q2_stride_next) + kStep);
    float bob = q.f(me, f_q2_bob), step = q.f(me, f_q2_bob_step);
    const bool turned = gc::step_q2_stride(bob, step, speed, ground, false);
    q.set_f(me, f_q2_bob, bob);
    q.set_f(me, f_q2_bob_step, step);
    if (!turned || !gc::q2_footstep_heard(static_cast<int>(when), ground, speed)) continue;
    const auto pick = static_cast<int>(q.random() * 4) & 3;
    q.sound(me, kChanBody, q2s(q, idx(gc::kQ2Footsteps[pick])), 1, kAttnNorm);
  }
}

bool q2_obituary(A& q, int32_t targ, int32_t attacker) {
  if (!on(q.g(g_q2_arms))) return false;
  const int32_t how = q.i(targ, f_deathtype);
  for (int a = 0; a < gc::kQ2ArmCount; a++)
    for (const bool blast : {false, true}) {
      const gc::Q2ArmWords w = gc::q2_arm_words(static_cast<gc::Q2ArmId>(a), blast);
      if (w.words == nullptr || !q.is(how, w.words)) continue;
      say_all(q, kPrintMedium,
              {q.i(targ, q.L.f.netname), q.s(s_space), how, q.s(s_space),
               q.i(attacker, q.L.f.netname), q2s(q, idx(w.tail)), q.s(s_nl)});
      return true;
    }
  return false;
}

// void() q2_bolt_touch: a bolt's hit, or its spark on a wall.
void q2_bolt_touch(A& q) {
  const int32_t me = q.self();
  if (q.other() == q.i(me, q.L.f.owner)) return;
  if (q.pointcontents(q.v(me, q.L.f.origin)) == kContentSky) {
    q.remove(me);
    return;
  }
  const int a = static_cast<int>(q.f(me, f_q2_arm));
  const Vec3 back = q.normalize(q.v(me, q.L.f.velocity)) * -1.0f;
  if (q.f(q.other(), q.L.f.takedamage) != 0) {
    effect_at(q, idx(gc::kQ2HitBlood), q.v(me, q.L.f.origin), back);
    hit(q, q.other(), me, q.i(me, q.L.f.owner), q.f(me, f_dmg), static_cast<float>(arm(a).kick), a,
        false);
  } else {
#if defined(SE_RULES_CAMPAIGN)
    sq_note_impact(q, me, q.v(me, q.L.f.origin));  // Quake II's monsters may hear where it landed
#endif
    effect_at(q, idx(gc::kQ2HitBolt), q.v(me, q.L.f.origin), back);
  }
  q.remove(me);
}

// void() q2_rocket_touch: the hit (no knockback of its own), then the blast, sparing the one hit.
void q2_rocket_touch(A& q) {
  const int32_t me = q.self();
  if (q.other() == q.i(me, q.L.f.owner)) return;
  if (q.pointcontents(q.v(me, q.L.f.origin)) == kContentSky) {
    q.remove(me);
    return;
  }
  const int a = static_cast<int>(q.f(me, f_q2_arm));
  const int32_t owner = q.i(me, q.L.f.owner);
  if (q.f(q.other(), q.L.f.takedamage) != 0)
    hit(q, q.other(), me, owner, q.f(me, f_dmg), 0, a, false);
  q.set_v(me, q.L.f.origin, q.v(me, q.L.f.origin) - q.normalize(q.v(me, q.L.f.velocity)) * 8);
  q2_radius(q, me, owner, gc::kQ2RocketSplash, q.other(), gc::kQ2RocketRadius, a);
#if defined(SE_RULES_CAMPAIGN)
  sq_note_impact(q, me, q.v(me, q.L.f.origin));  // Quake II's monsters may hear where it landed
#endif
  const bool wet = q.pointcontents(q.v(me, q.L.f.origin)) <= -3;
  effect_at(q, idx(wet ? gc::kQ2RocketBlastWater : gc::kQ2RocketBlast), q.v(me, q.L.f.origin),
            {0, 0, 1});
  sound_at(q, me, kChanAuto, idx(gc::kQ2RocketBoom), 0);
  q.remove(me);
}

// void() q2_grenade_touch: a bounce off the world, a blast on what can be hurt.
void q2_grenade_touch(A& q) {
  const int32_t me = q.self();
  if (q.other() == q.i(me, q.L.f.owner)) return;
  if (q.pointcontents(q.v(me, q.L.f.origin)) == kContentSky) {
    q.remove(me);
    return;
  }
  if (q.f(q.other(), q.L.f.takedamage) == 0) {
    sound_at(q, me, kChanVoice, idx(gc::kQ2GrenadeBounce), 0);
    if (same_vec(q.v(me, q.L.f.velocity), {0, 0, 0})) q.set_v(me, q.L.f.avelocity, {0, 0, 0});
    return;
  }
  q.set_i(me, q.L.f.enemy, q.other());
  q2_grenade_blast(q);
}

// void() q2_grenade_blast: on its fuse or a body: the one it hit takes the blast whole by its
// distance, everything else near the splash.
void q2_grenade_blast(A& q) {
  const int32_t me = q.self();
  const int a = static_cast<int>(q.f(me, f_q2_arm));
  const int32_t owner = q.i(me, q.L.f.owner);
  const int32_t enemy = q.i(me, q.L.f.enemy);
  const float points = q.f(me, f_dmg);
  if ((enemy != 0) && (q.f(enemy, q.L.f.takedamage) != 0)) {
    const Vec3 mid =
        q.v(enemy, q.L.f.origin) + (q.v(enemy, q.L.f.mins) + q.v(enemy, q.L.f.maxs)) * 0.5f;
    const float d = points - 0.5f * q.vlen(q.v(me, q.L.f.origin) - mid);
    kill_words(q, enemy, a, false);
    blow_kick(q, d);
    damage(q, enemy, me, owner, d);
    blow_done(q);
  }
  q2_radius(q, me, owner, points, enemy, gc::kQ2GrenadeRadius, a);
#if defined(SE_RULES_CAMPAIGN)
  sq_note_impact(q, me, q.v(me, q.L.f.origin));  // Quake II's monsters may hear where it landed
#endif
  const bool wet = q.pointcontents(q.v(me, q.L.f.origin)) <= -3;
  effect_at(q, idx(wet ? gc::kQ2GrenadeBlastWater : gc::kQ2GrenadeBlast), q.v(me, q.L.f.origin),
            {0, 0, 1});
  sound_at(q, me, kChanAuto, idx(gc::kQ2GrenadeBoom), 0);
  q.remove(me);
}

// void() q2_sound_later: a helper's sample on its owner, then gone.
void q2_sound_later(A& q) {
  const int32_t me = q.self();
  const int32_t owner = q.i(me, q.L.f.owner);
  if (owner != 0)
    q.sound(owner, kChanAuto, q2s(q, static_cast<int>(q.f(me, f_q2_sound))), 1, kAttnNorm);
  q.remove(me);
}

}  // namespace shooters_edge::server::rules::SE_RULES_NS
