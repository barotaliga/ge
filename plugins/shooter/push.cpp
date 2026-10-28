// The pusher's frame and its push (push.hpp states the rules).
//
// ge:summary The pusher's movetime and think, and the push that carries, shoves or is blocked
// ge:keywords physics_pusher push_move movetime ltime moved blocked restore
// ge:skill shooter
// ge:test tests/shooter_push_test.cpp
#include "shooter/push.hpp"

#include "shooter/trace.hpp"

namespace ge::shooter {

namespace {

constexpr float kThinkMoveEpsilon = 1.0f / 64;  // a think's move worth pushing, units

Edict& ent(Server& sv, int e) { return sv.edicts[static_cast<size_t>(e)]; }

bool zero(Vec3f v) { return v.x == 0 && v.y == 0 && v.z == 0; }

bool test_without(Server& sv, int pusher, int e) {
  Edict& p = ent(sv, pusher);
  const int solid = p.solid;
  p.solid = kSolidNot;
  const bool stuck = test_entity_position(sv, e);
  p.solid = solid;
  return stuck;
}

}  // namespace

bool push_move(Server& sv, int pusher, Vec3f move) {
  sv.counts.pushes++;
  Vec3f mins, maxs;
  Vec3f pushorig;
  {
    Edict& p = ent(sv, pusher);
    mins = p.absmin + move;
    maxs = p.absmax + move;
    pushorig = p.origin;
    p.origin = p.origin + move;
  }
  link_edict(sv, pusher, false);

  int moved[kMaxEdicts];
  Vec3f moved_from[kMaxEdicts];
  int num_moved = 0;
  for (int e = 1; e < sv.num_edicts; e++) {
    {
      const Edict& c = ent(sv, e);
      if (c.free || c.movetype == kMovetypePush || c.movetype == kMovetypeNone ||
          c.movetype == kMovetypeNoclip)
        continue;
    }
    if (test_without(sv, pusher, e)) continue;  // already stuck in something else

    Edict& c = ent(sv, e);
    const bool rider = (c.flags & kFlagOnGround) != 0 && c.groundentity == pusher;
    if (!rider) {
      if (c.absmin.x >= maxs.x || c.absmin.y >= maxs.y || c.absmin.z >= maxs.z ||
          c.absmax.x <= mins.x || c.absmax.y <= mins.y || c.absmax.z <= mins.z)
        continue;
      if (!test_entity_position(sv, e)) continue;
    }

    if (c.movetype != kMovetypeWalk) c.flags &= ~kFlagOnGround;
    const Vec3f from = c.origin;
    moved[num_moved] = e;
    moved_from[num_moved] = from;
    num_moved++;
    c.origin = c.origin + move;
    if (!test_entity_position(sv, e)) {
      link_edict(sv, e, false);
      continue;
    }

    ent(sv, e).origin = from;
    if (!test_entity_position(sv, e)) {
      num_moved--;
      continue;
    }

    Edict& stuck = ent(sv, e);
    if (stuck.mins.x == stuck.maxs.x) {  // a point: left where it is
      link_edict(sv, e, false);
      continue;
    }
    if (stuck.solid == kSolidNot || stuck.solid == kSolidTrigger) {  // a corpse: flattened
      stuck.mins.x = 0;
      stuck.mins.y = 0;
      stuck.maxs = stuck.mins;
      stuck.size = {};
      continue;
    }

    // Blocked: everything goes back.
    sv.counts.pushes_blocked++;
    ent(sv, pusher).origin = pushorig;
    link_edict(sv, pusher, false);
    const Func blocked = ent(sv, pusher).blocked;
    if (blocked != kNoFunc) {
      sv.g.self = pusher;
      sv.g.other = e;
      sv.g.time = static_cast<float>(sv.time);
      call_program(sv, blocked);
    }
    for (int i = 0; i < num_moved; i++) {
      ent(sv, moved[i]).origin = moved_from[i];
      link_edict(sv, moved[i], false);
    }
    return false;
  }
  return true;
}

void physics_pusher(Server& sv, int e, double dt) {
  const auto fdt = static_cast<float>(dt);
  const float oldltime = ent(sv, e).ltime;
  const float thinktime = ent(sv, e).nextthink;
  float movetime = fdt;
  if (thinktime < oldltime + fdt) {
    movetime = thinktime - oldltime;
    if (movetime < 0) movetime = 0;
  }
  if (movetime != 0) {
    const Vec3f v = ent(sv, e).velocity;
    if (zero(v)) {
      ent(sv, e).ltime = ent(sv, e).ltime + movetime;
    } else if (push_move(sv, e, v * movetime)) {
      ent(sv, e).ltime = ent(sv, e).ltime + movetime;
    }
  }
  if (ent(sv, e).free) return;

  if (thinktime > oldltime && thinktime <= ent(sv, e).ltime) {
    const Vec3f oldorg = ent(sv, e).origin;
    ent(sv, e).nextthink = 0;
    sv.g.time = static_cast<float>(sv.time);
    sv.g.self = e;
    sv.g.other = 0;
    sv.counts.thinks++;
    call_program(sv, ent(sv, e).think);
    if (ent(sv, e).free) return;
    const Vec3f move = ent(sv, e).origin - oldorg;
    if (length(move) > kThinkMoveEpsilon) {
      set_origin(sv, e, oldorg);  // back, relinked, so the push starts from where it was
      push_move(sv, e, move);
    }
  }
}

}  // namespace ge::shooter
