// The physics frame's rules in their order (physics.hpp states them).
//
// ge:summary The physics frame: thinking, the toss, the step's fly-move, noclip and water
// ge:keywords physics_frame run_entity run_think physics_toss physics_step fly_move push_entity
// ge:keywords gravity bound_velocity check_water_transition run_new_missile
// ge:skill shooter
// ge:test tests/shooter_physics_test.cpp tests/shooter_toss_recording_test.cpp
#include "shooter/physics.hpp"

#include "shooter/push.hpp"
#include "shooter/slide.hpp"
#include "shooter/trace.hpp"

namespace ge::shooter {

namespace {

constexpr double kFloorZ = 0.7;    // a plane whose normal z is above this is a floor
constexpr float kBounceRest = 60;  // a bouncing thing rests on a floor below this speed up
constexpr float kBounce = 1.5f;    // a bouncing thing's rebound
constexpr int kMaxBumps = 4;
constexpr int kMaxPlanes = 5;

Edict& ent(Server& sv, int e) { return sv.edicts[static_cast<size_t>(e)]; }

// velocity.z -= factor x sv_gravity x dt: the product in float, times dt in double, the
// difference in double rounded to float.
void add_gravity(Server& sv, Edict& ed, double dt) {
  const float factor = ed.gravity != 0 ? ed.gravity : 1.0f;
  const float g = factor * sv.set.move.gravity;
  ed.velocity.z = static_cast<float>(static_cast<double>(ed.velocity.z) - g * dt);
}

int trace_kind(const Edict& ed) {
  if (ed.movetype == kMovetypeFlyMissile) return kTraceMissile;
  if (ed.solid == kSolidTrigger || ed.solid == kSolidNot) return kTraceNoMonsters;
  return kTraceNormal;
}

// Moves the entity by `push` as far as it goes, relinks it with its trigger touches and, on a
// hit, runs both touches.
HullTrace push_entity(Server& sv, int e, Vec3f push) {
  Edict& ed = ent(sv, e);
  const Vec3f end = ed.origin + push;
  const HullTrace tr = trace_move(sv, ed.origin, ed.mins, ed.maxs, end, trace_kind(ed), e);
  ed.origin = tr.endpos;
  link_edict(sv, e, true);
  if (tr.ent >= 0) impact(sv, e, tr.ent);
  return tr;
}

void physics_none(Server& sv, int e, double dt) { run_think(sv, e, dt); }

void physics_noclip(Server& sv, int e, double dt) {
  if (!run_think(sv, e, dt)) return;
  Edict& ed = ent(sv, e);
  const auto t = static_cast<float>(dt);
  ed.angles = mad(ed.angles, t, ed.avelocity);
  ed.origin = mad(ed.origin, t, ed.velocity);
  link_edict(sv, e, false);
}

}  // namespace

bool run_think(Server& sv, int e, double dt) {
  for (;;) {
    Edict& ed = ent(sv, e);
    const float thinktime = ed.nextthink;
    if (thinktime <= 0 || static_cast<double>(thinktime) > sv.time + dt) return true;
    const float used =
        static_cast<double>(thinktime) < sv.time ? static_cast<float>(sv.time) : thinktime;
    ed.nextthink = 0;
    sv.g.time = used;
    sv.g.self = e;
    sv.g.other = 0;
    sv.counts.thinks++;
    call_program(sv, ed.think);
    if (ent(sv, e).free) return false;
    // Again within this frame only when the think made progress.
    if (!(ent(sv, e).nextthink > used)) return true;
  }
}

void bound_velocity(Server& sv, int e) {
  Edict& ed = ent(sv, e);
  for (int i = 0; i < 3; i++) {
    if (ed.velocity[i] != ed.velocity[i]) ed.velocity[i] = 0;
    if (ed.origin[i] != ed.origin[i]) ed.origin[i] = 0;
  }
  const float len = length(ed.velocity);
  if (len > sv.set.maxvelocity) ed.velocity = ed.velocity * (sv.set.maxvelocity / len);
}

void check_water_transition(Server& sv, int e) {
  Edict& ed = ent(sv, e);
  const int cont = world_contents(sv, ed.origin);
  if (ed.watertype == 0) {  // just spawned: take the contents silently
    ed.watertype = cont;
    ed.waterlevel = 1;
    return;
  }
  if (cont <= kContentsWater) {
    if (ed.watertype == kContentsEmpty) start_sound(sv, e, 0, kSoundSplash, 1, 1);
    ed.watertype = cont;
    ed.waterlevel = 1;
  } else {
    if (ed.watertype != kContentsEmpty) start_sound(sv, e, 0, kSoundSplash, 1, 1);
    ed.watertype = kContentsEmpty;
    ed.waterlevel = cont;
  }
}

void physics_toss(Server& sv, int e, double dt, HullTrace* last) {
  if (last != nullptr) {
    *last = HullTrace{};
    last->allsolid = false;
  }
  if (!run_think(sv, e, dt)) return;
  {
    Edict& ed = ent(sv, e);
    if (ed.velocity.z > 0) ed.flags &= ~kFlagOnGround;
    if ((ed.flags & kFlagOnGround) != 0) return;
  }
  bound_velocity(sv, e);
  Edict& ed = ent(sv, e);
  if (ed.movetype != kMovetypeFly && ed.movetype != kMovetypeFlyMissile) add_gravity(sv, ed, dt);
  const auto t = static_cast<float>(dt);
  ed.angles = mad(ed.angles, t, ed.avelocity);
  const HullTrace tr = push_entity(sv, e, ed.velocity * t);
  if (last != nullptr) *last = tr;
  if (tr.fraction == 1 || ent(sv, e).free) return;

  Edict& me = ent(sv, e);
  const bool bounce = me.movetype == kMovetypeBounce;
  me.velocity = clip_velocity(me.velocity, tr.plane.normal, bounce ? kBounce : 1.0f);
  if (tr.plane.normal.z > kFloorZ && (!bounce || me.velocity.z < kBounceRest)) {
    me.flags |= kFlagOnGround;
    me.groundentity = tr.ent;
    me.velocity = {};
    me.avelocity = {};
  }
  check_water_transition(sv, e);
}

int fly_move(Server& sv, int e, float time) {
  int blocked = 0;
  const Vec3f primal = ent(sv, e).velocity;
  Vec3f original = primal;
  Vec3f planes[kMaxPlanes];
  int numplanes = 0;
  float time_left = time;
  const int kind = ent(sv, e).solid == kSolidNot ? kTraceNoMonsters : kTraceNormal;

  for (int bump = 0; bump < kMaxBumps; bump++) {
    Edict& ed = ent(sv, e);
    const Vec3f end = mad(ed.origin, time_left, ed.velocity);
    const HullTrace tr = trace_move(sv, ed.origin, ed.mins, ed.maxs, end, kind, e);
    if (tr.allsolid) {
      ed.velocity = {};
      return 3;
    }
    if (tr.fraction > 0) {
      ed.origin = tr.endpos;
      original = ed.velocity;
      numplanes = 0;
    }
    if (tr.fraction == 1) break;

    if (tr.plane.normal.z > kFloorZ) {
      blocked |= 1;
      if (ent(sv, tr.ent).solid == kSolidBsp) {
        ed.flags |= kFlagOnGround;
        ed.groundentity = tr.ent;
      }
    }
    if (tr.plane.normal.z == 0) blocked |= 2;
    impact(sv, e, tr.ent);
    if (ent(sv, e).free) break;

    Edict& me = ent(sv, e);
    time_left -= time_left * tr.fraction;
    if (numplanes >= kMaxPlanes) {
      me.velocity = {};
      return 3;
    }
    planes[numplanes++] = tr.plane.normal;

    Vec3f v;
    int i = 0;
    for (; i < numplanes; i++) {
      v = clip_velocity(original, planes[i], 1);
      int j = 0;
      for (; j < numplanes; j++)
        if (j != i && dot(v, planes[j]) < 0) break;
      if (j == numplanes) break;
    }
    if (i == numplanes) {
      if (numplanes != 2) {
        me.velocity = {};
        return 7;
      }
      const Vec3f dir = cross(planes[0], planes[1]);
      v = dir * dot(dir, v);
    }
    me.velocity = v;
    if (dot(me.velocity, primal) <= 0) {
      me.velocity = {};
      return blocked;
    }
  }
  return blocked;
}

void physics_step(Server& sv, int e, double dt) {
  {
    Edict& ed = ent(sv, e);
    if ((ed.flags & (kFlagOnGround | kFlagFly | kFlagSwim)) == 0) {
      const bool hitsound = ed.velocity.z < sv.set.move.gravity * -0.1;
      add_gravity(sv, ed, dt);
      bound_velocity(sv, e);
      fly_move(sv, e, static_cast<float>(dt));
      if (ent(sv, e).free) return;
      link_edict(sv, e, true);
      if (ent(sv, e).free) return;
      if ((ent(sv, e).flags & kFlagOnGround) != 0 && hitsound)
        start_sound(sv, e, 0, kSoundLand, 1, 1);
    }
  }
  if (!run_think(sv, e, dt)) return;
  check_water_transition(sv, e);
}

void run_entity(Server& sv, int e, double dt) {
  Edict& ed = ent(sv, e);
  if (ed.free || ed.last_run == sv.time) return;
  ed.last_run = sv.time;
  switch (ed.movetype) {
    case kMovetypePush:
      physics_pusher(sv, e, dt);
      break;
    case kMovetypeNone:
    case kMovetypeLock:
      physics_none(sv, e, dt);
      break;
    case kMovetypeNoclip:
      physics_noclip(sv, e, dt);
      break;
    case kMovetypeStep:
      physics_step(sv, e, dt);
      break;
    case kMovetypeToss:
    case kMovetypeBounce:
    case kMovetypeFly:
    case kMovetypeFlyMissile:
      physics_toss(sv, e, dt);
      break;
    default:  // walk outside a player's slot: nothing moves it here
      break;
  }
}

void run_new_missile(Server& sv) {
  const int n = sv.g.newmis;
  if (n == 0) return;
  sv.g.newmis = 0;
  if (n < 0 || n >= sv.num_edicts || ent(sv, n).free) return;
  sv.counts.new_missiles++;
  run_entity(sv, n, kNewMissileDt);
}

void physics_frame(Server& sv, double frametime) {
  sv.counts.physics_frames++;
  sv.g.frametime = static_cast<float>(frametime);
  sv.g.self = 0;
  sv.g.other = 0;
  sv.g.time = static_cast<float>(sv.time);
  call_program(sv, sv.program.start_frame);

  // num_edicts is read each turn: an entity spawned this frame runs this frame.
  for (int i = 0; i < sv.num_edicts; i++) {
    if (ent(sv, i).free) continue;
    if (sv.g.force_retouch != 0) link_edict(sv, i, true);
    if (i >= 1 && i <= sv.set.maxclients) continue;
    run_entity(sv, i, frametime);
    run_new_missile(sv);
  }
  if (sv.g.force_retouch != 0) sv.g.force_retouch = sv.g.force_retouch - 1;
}

}  // namespace ge::shooter
