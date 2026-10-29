// The slide's bumps and the step's two tries (slide.hpp states the rules).
//
// ge:summary The slide's bump loop, the crease, and the step-slide's raised try
// ge:keywords slide_move step_slide_move clip_velocity bumps planes crease stepsize
// ge:skill shooter
// ge:test tests/shooter_slide_test.cpp tests/shooter_pmove_test.cpp
#include "shooter/slide.hpp"

namespace ge::shooter {

namespace {

constexpr int kMaxBumps = 4;
constexpr int kMaxPlanes = 5;
constexpr float kStepHeight = 18;
constexpr double kFloorZ = 0.7;  // a plane whose normal z reaches this is ground

}  // namespace

Vec3f clip_velocity(Vec3f v, Vec3f normal, float overbounce) {
  const float backoff = dot(v, normal) * overbounce;
  Vec3f out;
  for (int i = 0; i < 3; i++) {
    out[i] = v[i] - normal[i] * backoff;
    if (out[i] > -0.1 && out[i] < 0.1) out[i] = 0;
  }
  return out;
}

int slide_move(Mover& m) {
  MoveState& s = m.s;
  int blocked = 0;
  const Vec3f primal = s.velocity;
  Vec3f planes[kMaxPlanes];
  int numplanes = 0;
  float time_left = m.dt;

  for (int bump = 0; bump < kMaxBumps; bump++) {
    const Vec3f end = mad(s.origin, time_left, s.velocity);
    const HullTrace tr = player_trace(m.world, s.origin, end);
    if (tr.startsolid || tr.allsolid) {
      s.velocity = {};
      return kBlockedFloor | kBlockedStep;
    }
    if (tr.fraction > 0) {
      s.origin = tr.endpos;
      numplanes = 0;
    }
    if (tr.fraction == 1) break;

    m.touches.add(tr.ent);
    const float nz = tr.plane.normal.z;
    if (nz >= kFloorZ) {
      blocked |= kBlockedFloor;
      m.floor_ent = tr.ent;
      m.floor_normal = tr.plane.normal;
    } else if (nz == 0) {
      blocked |= kBlockedStep;
    } else {
      blocked |= kBlockedOther;
    }

    time_left -= time_left * tr.fraction;
    if (numplanes >= kMaxPlanes) {
      s.velocity = {};
      break;
    }
    planes[numplanes++] = tr.plane.normal;

    // The first plane whose clip leaves the velocity out of every other plane.
    Vec3f v;
    int i = 0;
    for (; i < numplanes; i++) {
      v = clip_velocity(primal, planes[i], 1);
      int j = 0;
      for (; j < numplanes; j++)
        if (j != i && dot(v, planes[j]) < 0) break;
      if (j == numplanes) break;
    }
    if (i == numplanes) {
      if (numplanes != 2) {
        s.velocity = {};
        break;
      }
      const Vec3f dir = cross(planes[0], planes[1]);
      v = dir * dot(dir, v);
    }
    s.velocity = v;
    if (dot(s.velocity, primal) <= 0) {
      s.velocity = {};
      break;
    }
  }
  if (s.waterjump_time > 0) s.velocity = primal;
  return blocked;
}

int step_slide_move(Mover& m, bool in_air) {
  MoveState& s = m.s;
  const Vec3f start_o = s.origin;
  const Vec3f start_v = s.velocity;
  const int blocked = slide_move(m);
  if (blocked == 0) return blocked;

  float stepsize = kStepHeight;
  if (in_air) {
    if ((blocked & kBlockedStep) == 0) return blocked;
    // The ground under the player bounds the climb: the whole step stays within 18 of it.
    const Vec3f from = start_v.z < 0 ? s.origin : start_o;
    const HullTrace tr = player_trace(m.world, from, {from.x, from.y, from.z - kStepHeight});
    if (tr.fraction == 1 || tr.plane.normal.z < kFloorZ) return blocked;
    stepsize = kStepHeight - (from.z - tr.endpos.z);
  }

  const Vec3f down_o = s.origin;
  const Vec3f down_v = s.velocity;

  // Up by the step, across again, and back down.
  s.origin = start_o;
  s.velocity = start_v;
  {
    const HullTrace tr =
        player_trace(m.world, s.origin, {s.origin.x, s.origin.y, s.origin.z + stepsize});
    if (!tr.startsolid && !tr.allsolid) s.origin = tr.endpos;
  }
  if (in_air && start_v.z < 0) s.velocity.z = 0;
  slide_move(m);

  bool use_down = false;
  {
    const HullTrace tr =
        player_trace(m.world, s.origin, {s.origin.x, s.origin.y, s.origin.z - stepsize});
    if (tr.fraction < 1 && tr.plane.normal.z < kFloorZ) {
      use_down = true;
    } else {
      if (!tr.startsolid && !tr.allsolid) s.origin = tr.endpos;
      if (s.origin.z < start_o.z) use_down = true;
    }
  }
  if (!use_down) {
    const float dx = down_o.x - start_o.x, dy = down_o.y - start_o.y;
    const float ux = s.origin.x - start_o.x, uy = s.origin.y - start_o.y;
    if (dx * dx + dy * dy >= ux * ux + uy * uy) use_down = true;
  }
  if (use_down) {
    s.origin = down_o;
    s.velocity = down_v;
    return blocked;
  }

  s.velocity.z = down_v.z;
  if (in_air && s.ground == -1 && s.waterlevel < 2) {
    const float keep = 1 - 0.01f * (s.origin.z - start_o.z);
    s.velocity.x = s.velocity.x * keep;
    s.velocity.y = s.velocity.y * keep;
  }
  return blocked;
}

}  // namespace ge::shooter
