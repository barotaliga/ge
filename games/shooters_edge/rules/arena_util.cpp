// The arena's helpers every other rule uses: removing an entity later, the direction a map's angle
// means, a mover's straight move to a point at a speed, firing an entity's targets, the effects of
// a blast and a beam, a random float in -1..1 and a decimal read as a number.
//
// ge:summary The arena's helpers in C++: movers' moves, targets fired, effects, numbers from text
// ge:keywords arena native-arena calc_move use_targets set_movedir effect_explosion number_of
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#include <cstdint>
#include <string_view>

#include "shooters_edge/rules/arena_decl.hpp"

namespace shooters_edge::server::arena {

// void() noop: nothing.
void noop(A&) {}

// void() remove_self: remove(self).
void remove_self(A& q) { q.remove(q.self()); }

// void() set_movedir: the direction the map's "angle" key means (-1 up, -2 down, else a yaw);
// the angles cleared so a brush entity is not drawn turned.
void set_movedir(A& q) {
  const Vec3 a = q.v(q.self(), q.L.f.angles);
  if (same_vec(a, {0, -1, 0})) {
    q.set_v(q.self(), q.L.f.movedir, {0, 0, 1});
  } else if (same_vec(a, {0, -2, 0})) {
    q.set_v(q.self(), q.L.f.movedir, {0, 0, -1});
  } else {
    q.makevectors(a);
    q.set_v(q.self(), q.L.f.movedir, q.v_forward());
  }
  q.set_v(q.self(), q.L.f.angles, {0, 0, 0});
}

// void() calc_move_arrive: on the destination, still, and the move's done function called.
void calc_move_arrive(A& q) {
  q.setorigin(q.self(), q.v(q.self(), f_finaldest));
  q.set_v(q.self(), q.L.f.velocity, {0, 0, 0});
  q.set_f(q.self(), q.L.f.nextthink, -1);
  const int32_t done = q.i(q.self(), f_move_done);
  if (done != 0) q.call(done);
}

// calc_move(dest, spd, done): `self` (a pusher, on its own clock .ltime) to `dest` at `spd` units
// a second, then `done`; the move ends exactly on `dest` at a think.
void calc_move(A& q, Vec3 dest, float spd, int32_t done) {
  if (spd == 0) q.objerror({q.s(s_calc_move_no_speed), q.i(q.self(), q.L.f.classname)});
  q.set_i(q.self(), f_move_done, done);
  q.set_v(q.self(), f_finaldest, dest);
  q.set_i(q.self(), q.L.f.think, q.fn(fn_calc_move_arrive));
  const Vec3 delta = dest - q.v(q.self(), q.L.f.origin);
  if (same_vec(delta, {0, 0, 0})) {
    q.set_f(q.self(), q.L.f.nextthink, q.f(q.self(), q.L.f.ltime) + 0.1f);
    return;
  }
  const float travel = q.vlen(delta) / spd;
  if (travel < 0.1f) {
    q.set_v(q.self(), q.L.f.velocity, {0, 0, 0});
    q.set_f(q.self(), q.L.f.nextthink, q.f(q.self(), q.L.f.ltime) + 0.1f);
    return;
  }
  q.set_f(q.self(), q.L.f.nextthink, q.f(q.self(), q.L.f.ltime) + travel);
  q.set_v(q.self(), q.L.f.velocity, delta * (1 / travel));
}

// void() delayed_use: the targets fired for the activator kept, the helper gone.
void delayed_use(A& q) {
  q.set_ge(g_activator, q.i(q.self(), q.L.f.enemy));
  use_targets(q);
  q.remove(q.self());
}

// void() use_targets: `self`'s targets fired for `activator`: after .delay through a helper, its
// message to a player who set it off, every .killtarget removed, every .target used.
void use_targets(A& q) {
  const int32_t me = q.self();
  if (on(q.f(me, f_delay))) {
    const int32_t t = q.spawn();
    q.set_i(t, q.L.f.classname, q.s(s_delayed_use));
    q.set_f(t, q.L.f.nextthink, q.time() + q.f(me, f_delay));
    q.set_i(t, q.L.f.think, q.fn(fn_delayed_use));
    q.set_i(t, q.L.f.enemy, q.ge(g_activator));
    q.set_i(t, q.L.f.message, q.i(me, q.L.f.message));
    q.set_i(t, f_killtarget, q.i(me, f_killtarget));
    q.set_i(t, q.L.f.target, q.i(me, q.L.f.target));
    return;
  }
  const int32_t activator = q.ge(g_activator);
  if (q.is(q.i(activator, q.L.f.classname), "player") & !q.is(q.i(me, q.L.f.message), "")) {
    q.centerprint(activator, {q.i(me, q.L.f.message)});
  }
  if (!q.is(q.i(q.self(), f_killtarget), "")) {
    int32_t t = q.find(q.world(), q.L.f.targetname, q.i(q.self(), f_killtarget));
    while (t != 0) {
      q.remove(t);
      t = q.find(t, q.L.f.targetname, q.i(q.self(), f_killtarget));
    }
  }
  if (!q.is(q.i(q.self(), q.L.f.target), "")) {
    const int32_t was_self = q.self();
    const int32_t was_other = q.other();
    int32_t t = q.find(q.world(), q.L.f.targetname, q.i(q.self(), q.L.f.target));
    while (t != 0) {
      q.set_self(t);
      q.set_other(was_self);
      const int32_t use = q.i(q.self(), q.L.f.use);
      if (use != 0) q.call(use);
      q.set_self(was_self);
      q.set_other(was_other);
      t = q.find(t, q.L.f.targetname, q.i(q.self(), q.L.f.target));
    }
  }
}

// A temporary effect's head: the message and its kind, into the multicast buffer.
void effect_head(A& q, float kind) {
  q.WriteByte(kMsgEffect, kSvcTempentity);
  q.WriteByte(kMsgEffect, kind);
}

void write_vec(A& q, Vec3 v) {
  q.WriteCoord(kMsgEffect, v.x);
  q.WriteCoord(kMsgEffect, v.y);
  q.WriteCoord(kMsgEffect, v.z);
}

// void(vector org) effect_explosion.
void effect_explosion(A& q, Vec3 org) {
  effect_head(q, kTeExplosion);
  write_vec(q, org);
  effect_send(q, org, kMulticastPhs);
}

// void(vector from, vector to) effect_beam: the arc gun's beam, drawn as `self`'s.
void effect_beam(A& q, Vec3 from, Vec3 to) {
  effect_head(q, kTeLightning2);
  q.WriteEntity(kMsgEffect, q.self());
  write_vec(q, from);
  write_vec(q, to);
  effect_send(q, from, kMulticastPhs);
}

// float() crandom: 2 * random() - 1.
float crandom(A& q) { return 2 * q.random() - 1; }

float min_of(float a, float b) { return a < b ? a : b; }

// float(string s) number_of: a decimal ("-12", "0.04", "2.5") read as the float nearest the text:
// the digits make a whole number, divided once by the power of ten its fraction needs.
float number_of(A& q, int32_t s) {
  const char* c = q.text(s);
  const std::string_view t(c != nullptr ? c : "");
  size_t i = 0;
  float whole = 0;
  float scale = 1;
  bool minus = false;
  bool past_point = false;
  if (!t.empty() && t[0] == '-') {
    minus = true;
    i = 1;
  }
  for (; i < t.size(); i++) {
    if (t[i] == '.') {
      past_point = true;
    } else {
      if (t[i] < '0' || t[i] > '9') break;
      whole = whole * 10 + static_cast<float>(t[i] - '0');
      if (past_point) scale = scale * 10;
    }
  }
  if (minus) return (0 - whole) / scale;  // unary minus is 0 - x in the bytecode
  return whole / scale;
}

}  // namespace shooters_edge::server::arena
