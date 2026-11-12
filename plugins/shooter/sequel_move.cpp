// The sequel's player move, step by step (sequel_move.hpp). Precision: every operation is a
// 32-bit float one done in the order written unless a comment says a double enters; a literal
// written as a double in a comparison (0.7, 0.1) is compared as a double.
//
// ge:summary The sequel's player move's steps: view, duck, categorise, jump, friction, slide, snap
// ge:keywords sequel-move categorise friction accelerate air-accelerate step-slide clip snap
// ge:keywords ladder current conveyor water-jump duck landing spectator fly dead
// ge:skill shooter
// ge:test tests/shooter_sequel_move_test.cpp
#include "shooter/sequel_move.hpp"

#include "brush/trace.hpp"
#include "core/num.hpp"

namespace ge::shooter {

namespace {

constexpr float kStepSize = 18;
constexpr float kStopSpeed = 100;
constexpr float kMaxSpeed = 300;
constexpr float kDuckSpeed = 100;
constexpr float kAccelerate = 10;
constexpr float kWaterAccelerate = 10;
constexpr float kFriction = 6;
constexpr float kWaterFriction = 1;
constexpr float kWaterSpeed = 400;
constexpr double kStopEpsilon = 0.1;
constexpr double kMinStepNormal = 0.7;
constexpr int kMaxClipPlanes = 5;
constexpr uint8_t kTimeFlags = kSqTimeWaterJump | kSqTimeLand | kSqTimeTeleport;

float fabs_f(float x) { return x < 0 ? -x : x; }

struct Move {
  Move(SequelMove& m, const SequelWorld& world) : pm(m), w(world) {}
  SequelMove& pm;
  const SequelWorld& w;
  Vec3f origin, velocity;
  Vec3f forward, right, up;
  float seconds = 0;
  Vec3f ground_normal;
  int32_t ground_surface = 0;
  int32_t ground_contents = 0;
  int16_t previous[3] = {};
  bool ladder = false;

  SequelTrace trace(Vec3f a, Vec3f b) const { return w.trace(w.ctx, a, pm.mins, pm.maxs, b); }
  int32_t contents(Vec3f p) const { return w.contents(w.ctx, p); }

  void touch(int entity) {
    if (entity >= 0 && pm.touches < kSqMaxTouch) pm.touch[pm.touches++] = entity;
  }

  // To a plane: back = (v · n) × overbounce; an axis left strictly within ±0.1 is 0.
  static Vec3f clip(Vec3f in, Vec3f n, float overbounce) {
    const float back = dot(in, n) * overbounce;
    Vec3f out;
    for (int i = 0; i < 3; ++i) {
      const float change = n[i] * back;
      out[i] = in[i] - change;
      if (out[i] > -kStopEpsilon && out[i] < kStopEpsilon) out[i] = 0;
    }
    return out;
  }

  // The slide: up to four bumps, five planes, the crease of two.
  void slide() {
    Vec3f planes[kMaxClipPlanes];
    int count = 0;
    const Vec3f primal = velocity;
    float left = seconds;
    for (int bump = 0; bump < 4; ++bump) {
      Vec3f end;
      for (int i = 0; i < 3; ++i) end[i] = origin[i] + left * velocity[i];
      const SequelTrace t = trace(origin, end);
      if (t.all_solid) {  // trapped: the slide ends here, with no primal-velocity rule
        velocity.z = 0;
        return;
      }
      if (t.fraction > 0) {
        origin = t.end;
        count = 0;
      }
      if (t.fraction == 1) break;
      touch(t.entity);
      left -= left * t.fraction;
      if (count >= kMaxClipPlanes) {
        velocity = {};
        break;
      }
      planes[count++] = t.normal;
      int i = 0;
      for (; i < count; ++i) {
        velocity = clip(velocity, planes[i], 1.01f);
        int j = 0;
        for (; j < count; ++j) {
          if (j != i && dot(velocity, planes[j]) < 0) break;
        }
        if (j == count) break;
      }
      if (i == count) {
        if (count != 2) {
          velocity = {};
          break;
        }
        const Vec3f dir = cross(planes[0], planes[1]);
        const float d = dot(dir, velocity);
        velocity = dir * d;
      }
      if (dot(velocity, primal) <= 0) {
        velocity = {};
        break;
      }
    }
    if (pm.s.time) velocity = primal;
  }

  // The step slide: slide; then from 18 up slide again and come down 18; keep the farther.
  void step_slide() {
    const Vec3f start_o = origin, start_v = velocity;
    slide();
    const Vec3f down_o = origin, down_v = velocity;
    Vec3f up_at = start_o;
    up_at.z += kStepSize;
    if (trace(up_at, up_at).all_solid) return;  // no room to step up
    origin = up_at;
    velocity = start_v;
    slide();
    Vec3f down = origin;
    down.z -= kStepSize;
    const SequelTrace t = trace(origin, down);
    if (!t.all_solid) origin = t.end;
    const Vec3f stepped = origin;
    const float down_dist = (down_o.x - start_o.x) * (down_o.x - start_o.x) +
                            (down_o.y - start_o.y) * (down_o.y - start_o.y);
    const float up_dist = (stepped.x - start_o.x) * (stepped.x - start_o.x) +
                          (stepped.y - start_o.y) * (stepped.y - start_o.y);
    if (down_dist > up_dist || t.normal.z < kMinStepNormal) {
      origin = down_o;
      velocity = down_v;
      return;
    }
    velocity.z = down_v.z;
  }

  void accelerate(Vec3f dir, float speed, float accel) {
    const float add = speed - dot(velocity, dir);
    if (add <= 0) return;
    float amount = accel * seconds * speed;
    if (amount > add) amount = add;
    for (int i = 0; i < 3; ++i) velocity[i] += amount * dir[i];
  }

  // The strafing rule: the gain is measured against a wish of at most 30, the amount against
  // the whole wish.
  void air_accelerate(Vec3f dir, float speed, float accel) {
    const float capped = speed > 30 ? 30.0f : speed;
    const float add = capped - dot(velocity, dir);
    if (add <= 0) return;
    float amount = accel * speed * seconds;
    if (amount > add) amount = add;
    for (int i = 0; i < 3; ++i) velocity[i] += amount * dir[i];
  }

  void friction() {
    const float speed = length(velocity);
    if (speed < 1) {
      velocity.x = 0;
      velocity.y = 0;
      return;
    }
    float drop = 0;
    if ((pm.ground_entity >= 0 && !(ground_surface & kSqSurfSlick)) || ladder) {
      const float control = speed < kStopSpeed ? kStopSpeed : speed;
      drop += control * kFriction * seconds;
    }
    if (pm.water_level && !ladder) {
      drop += speed * kWaterFriction * float(pm.water_level) * seconds;
    }
    float scale = speed - drop;
    if (scale < 0) scale = 0;
    scale /= speed;
    velocity = velocity * scale;
  }

  void currents(Vec3f& wish) const {
    if (ladder && fabs_f(velocity.z) <= 200) {
      // Looking 15 or more up or down while moving forward climbs that way; else the up move.
      const float pitch = pm.view_angles.x;
      const bool looks = (pitch <= -15 || pitch >= 15) && pm.cmd.forward > 0;
      const bool climbs = looks ? pitch <= -15 : pm.cmd.up > 0;
      const bool descends = looks ? pitch >= 15 : pm.cmd.up < 0;
      wish.z = climbs ? 200.0f : descends ? -200.0f : 0.0f;
      for (int i = 0; i < 2; ++i) {
        if (wish[i] < -25) wish[i] = -25;
        if (wish[i] > 25) wish[i] = 25;
      }
    }
    const auto flow = [](int32_t bits) {
      Vec3f v;
      if (bits & kSqCurrent0) v.x += 1;
      if (bits & kSqCurrent90) v.y += 1;
      if (bits & kSqCurrent180) v.x -= 1;
      if (bits & kSqCurrent270) v.y -= 1;
      if (bits & kSqCurrentUp) v.z += 1;
      if (bits & kSqCurrentDown) v.z -= 1;
      return v;
    };
    if (pm.water_type & kSqMaskCurrent) {
      const Vec3f v = flow(pm.water_type);
      float s = kWaterSpeed;
      if (pm.water_level == 1 && pm.ground_entity >= 0) s /= 2;
      wish = mad(wish, s, v);
    }
    if (pm.ground_entity >= 0) {
      const Vec3f v = flow(ground_contents);
      wish = mad(wish, 100.0f, v);
    }
  }

  void water_move() {
    const float fmove = pm.cmd.forward, smove = pm.cmd.side;
    Vec3f wish;
    for (int i = 0; i < 3; ++i) wish[i] = forward[i] * fmove + right[i] * smove;
    if (!pm.cmd.forward && !pm.cmd.side && !pm.cmd.up) {
      wish.z -= 60;
    } else {
      wish.z += float(pm.cmd.up);
    }
    currents(wish);
    Vec3f dir = wish;
    float speed = normalize(dir);
    if (speed > kMaxSpeed) speed = kMaxSpeed;
    speed *= 0.5f;
    accelerate(dir, speed, kWaterAccelerate);
    step_slide();
  }

  void air_move() {
    const float fmove = pm.cmd.forward, smove = pm.cmd.side;
    Vec3f wish;
    for (int i = 0; i < 2; ++i) wish[i] = forward[i] * fmove + right[i] * smove;
    wish.z = 0;
    currents(wish);
    Vec3f dir = wish;
    float speed = normalize(dir);
    const float cap = (pm.s.flags & kSqDucked) ? kDuckSpeed : kMaxSpeed;
    if (speed > cap) {
      wish = wish * (cap / speed);
      speed = cap;
    }
    const float g = float(pm.s.gravity);
    if (ladder) {
      accelerate(dir, speed, kAccelerate);
      if (wish.z == 0) {
        if (velocity.z > 0) {
          velocity.z -= g * seconds;
          if (velocity.z < 0) velocity.z = 0;
        } else {
          velocity.z += g * seconds;
          if (velocity.z > 0) velocity.z = 0;
        }
      }
      step_slide();
    } else if (pm.ground_entity >= 0) {
      velocity.z = 0;
      accelerate(dir, speed, kAccelerate);
      if (g > 0) {
        velocity.z = 0;
      } else {
        velocity.z -= g * seconds;
      }
      if (velocity.x == 0 && velocity.y == 0) return;
      step_slide();
    } else {
      if (pm.air_accelerate != 0) {
        air_accelerate(dir, speed, kAccelerate);
      } else {
        accelerate(dir, speed, 1);
      }
      velocity.z -= g * seconds;
      step_slide();
    }
  }

  void categorise() {
    Vec3f below = origin;
    below.z = float(double(origin.z) - 0.25);
    if (velocity.z > 180) {
      pm.s.flags &= uint8_t(~kSqOnGround);
      pm.ground_entity = -1;
    } else {
      const SequelTrace t = trace(origin, below);
      ground_normal = t.normal;
      ground_surface = t.surface_flags;
      ground_contents = t.contents;
      if (t.entity < 0 || (t.normal.z < kMinStepNormal && !t.start_solid)) {
        pm.ground_entity = -1;
        pm.s.flags &= uint8_t(~kSqOnGround);
      } else {
        pm.ground_entity = t.entity;
        if (pm.s.flags & kSqTimeWaterJump) {
          pm.s.flags &= uint8_t(~kTimeFlags);
          pm.s.time = 0;
        }
        if (!(pm.s.flags & kSqOnGround)) {
          pm.s.flags |= kSqOnGround;
          if (velocity.z < -200) {  // a long fall: no jump until the pause runs out
            pm.s.flags |= kSqTimeLand;
            pm.s.time = velocity.z < -400 ? 25 : 18;
          }
        }
      }
      touch(t.entity);
    }
    pm.water_level = 0;
    pm.water_type = 0;
    const int high = int(pm.view_height - pm.mins.z);  // 46 standing, 22 ducked
    const int mid = high / 2;
    Vec3f p = origin;
    p.z = origin.z + pm.mins.z + 1;
    const int32_t at_feet = contents(p);
    if (at_feet & kSqMaskWater) {
      pm.water_type = at_feet;
      pm.water_level = 1;
      p.z = origin.z + pm.mins.z + float(mid);
      if (contents(p) & kSqMaskWater) {
        pm.water_level = 2;
        p.z = origin.z + pm.mins.z + float(high);
        if (contents(p) & kSqMaskWater) pm.water_level = 3;
      }
    }
  }

  void special() {
    if (pm.s.time) return;
    ladder = false;
    Vec3f flat{forward.x, forward.y, 0};
    normalize(flat);
    const Vec3f spot = mad(origin, 1.0f, flat);
    const SequelTrace t = trace(origin, spot);
    if (t.fraction < 1 && (t.contents & kSqLadder)) ladder = true;
    if (pm.water_level != 2) return;
    Vec3f s = mad(origin, 30.0f, flat);
    s.z += 4;
    if (!(contents(s) & kSqSolid)) return;
    s.z += 16;
    if (contents(s)) return;
    velocity = flat * 50;
    velocity.z = 350;
    pm.s.flags |= kSqTimeWaterJump;
    pm.s.time = 255;
  }

  void jump() {
    if (pm.s.flags & kSqTimeLand) return;
    if (pm.cmd.up < 10) {
      pm.s.flags &= uint8_t(~kSqJumpHeld);
      return;
    }
    if (pm.s.flags & kSqJumpHeld) return;
    if (pm.s.type == SequelType::dead) return;
    if (pm.water_level >= 2) {
      pm.ground_entity = -1;
      if (velocity.z <= -300) return;
      if (pm.water_type == kSqWater) {
        velocity.z = 100;
      } else if (pm.water_type == kSqSlime) {
        velocity.z = 80;
      } else {
        velocity.z = 50;
      }
      return;
    }
    if (pm.ground_entity < 0) return;
    pm.s.flags |= kSqJumpHeld;
    pm.ground_entity = -1;
    velocity.z += 270;
    if (velocity.z < 270) velocity.z = 270;
  }

  void dead() {
    if (pm.ground_entity < 0) return;
    float speed = length(velocity);
    speed -= 20;
    if (speed <= 0) {
      velocity = {};
    } else {
      normalize(velocity);
      velocity = velocity * speed;
    }
  }

  // The spectator: its own friction (6 × 1.5), a three-axis wish, no collision.
  void fly() {
    pm.view_height = 22;
    const float speed = length(velocity);
    if (speed < 1) {
      velocity = {};
    } else {
      const auto fric = float(double(kFriction) * 1.5);
      const float control = speed < kStopSpeed ? kStopSpeed : speed;
      const float drop = control * fric * seconds;
      float scale = speed - drop;
      if (scale < 0) scale = 0;
      scale /= speed;
      velocity = velocity * scale;
    }
    const float fmove = pm.cmd.forward, smove = pm.cmd.side;
    normalize(forward);
    normalize(right);
    Vec3f wish;
    for (int i = 0; i < 3; ++i) wish[i] = forward[i] * fmove + right[i] * smove;
    wish.z += float(pm.cmd.up);
    Vec3f dir = wish;
    float wspeed = normalize(dir);
    if (wspeed > kMaxSpeed) wspeed = kMaxSpeed;
    accelerate(dir, wspeed, kAccelerate);
    origin = mad(origin, seconds, velocity);
  }

  bool good_at(const int16_t at[3]) const {
    if (pm.s.type == SequelType::spectator) return true;
    const Vec3f p{at[0] * 0.125f, at[1] * 0.125f, at[2] * 0.125f};
    return !trace(p, p).all_solid;
  }

  void snap() {
    for (int i = 0; i < 3; ++i) pm.s.velocity[i] = int16_t(int(velocity[i] * 8));
    int sign[3];
    int16_t base[3];
    for (int i = 0; i < 3; ++i) {
      sign[i] = origin[i] >= 0 ? 1 : -1;
      base[i] = int16_t(int(origin[i] * 8));
      if (double(base[i]) * 0.125 == double(origin[i])) sign[i] = 0;
    }
    static constexpr int kJitter[8] = {0, 4, 1, 2, 3, 5, 6, 7};  // bit 0 x, 1 y, 2 z
    for (const int bits : kJitter) {
      int16_t at[3];
      for (int i = 0; i < 3; ++i) at[i] = int16_t(base[i] + ((bits & (1 << i)) ? sign[i] : 0));
      if (good_at(at)) {
        for (int i = 0; i < 3; ++i) pm.s.origin[i] = at[i];
        return;
      }
    }
    for (int i = 0; i < 3; ++i) pm.s.origin[i] = previous[i];
  }

  void initial_snap() {
    static constexpr int kOffset[3] = {0, -1, 1};
    const int16_t* base = pm.s.origin;
    for (const int dz : kOffset) {
      for (const int dy : kOffset) {
        for (const int dx : kOffset) {
          const int16_t at[3] = {int16_t(base[0] + dx), int16_t(base[1] + dy),
                                 int16_t(base[2] + dz)};
          if (good_at(at)) {
            for (int i = 0; i < 3; ++i) {
              pm.s.origin[i] = at[i];
              previous[i] = at[i];
              origin[i] = at[i] * 0.125f;
            }
            return;
          }
        }
      }
    }
  }

  void box() {
    pm.mins = {-16, -16, -24};
    pm.maxs = {16, 16, 32};
    if (pm.s.type == SequelType::gib) {
      pm.mins.z = 0;
      pm.maxs.z = 16;
      pm.view_height = 8;
      return;
    }
    if (pm.s.type == SequelType::dead || (pm.cmd.up < 0 && (pm.s.flags & kSqOnGround))) {
      pm.s.flags |= kSqDucked;            // the dead always, the living when ducking on the ground
    } else if (pm.s.flags & kSqDucked) {  // stand up where the standing box fits
      if (!trace(origin, origin).all_solid) pm.s.flags &= uint8_t(~kSqDucked);
    }
    if (pm.s.flags & kSqDucked) {
      pm.maxs.z = 4;
      pm.view_height = -2;
    } else {
      pm.maxs.z = 32;
      pm.view_height = 22;
    }
  }

  void view() {
    const auto degrees = [](int v) { return float(double(v) * (360.0 / 65536)); };
    if (pm.s.flags & kSqTimeTeleport) {
      pm.view_angles = {0, degrees(int(pm.cmd.angles[1]) + int(pm.s.delta_angles[1])), 0};
    } else {
      for (int i = 0; i < 3; ++i) {
        const auto sum = int16_t(pm.cmd.angles[i] + pm.s.delta_angles[i]);
        pm.view_angles[i] = degrees(sum);
      }
      float& pitch = pm.view_angles.x;
      if (pitch > 89 && pitch < 180) {
        pitch = 89;
      } else if (pitch < 271 && pitch >= 180) {
        pitch = 271;
      }
    }
    const ViewAxes a = angle_vectors(pm.view_angles);
    forward = a.forward;
    right = a.right;
    up = a.up;
  }

  void run() {
    pm.touches = 0;
    pm.view_angles = {};
    pm.view_height = 0;
    pm.ground_entity = -1;
    pm.water_type = 0;
    pm.water_level = 0;
    pm.mins = {};
    pm.maxs = {};
    for (int i = 0; i < 3; ++i) {
      origin[i] = pm.s.origin[i] * 0.125f;
      velocity[i] = pm.s.velocity[i] * 0.125f;
      previous[i] = pm.s.origin[i];
    }
    seconds = float(double(pm.cmd.msec) * 0.001);
    view();

    if (pm.s.type == SequelType::spectator) {
      pm.mins = {-16, -16, -24};
      pm.maxs = {16, 16, 32};
      fly();
      snap();
      return;
    }
    if (pm.s.type >= SequelType::dead) {
      pm.cmd.forward = 0;
      pm.cmd.side = 0;
      pm.cmd.up = 0;
    }
    if (pm.s.type == SequelType::freeze) return;

    box();
    if (pm.changed) initial_snap();
    categorise();
    if (pm.s.type == SequelType::dead) dead();
    special();

    if (pm.s.time) {
      int step = pm.cmd.msec >> 3;
      if (!step) step = 1;
      if (step >= pm.s.time) {
        pm.s.flags &= uint8_t(~kTimeFlags);
        pm.s.time = 0;
      } else {
        pm.s.time = uint8_t(pm.s.time - step);
      }
    }

    if (pm.s.flags & kSqTimeTeleport) {
      // held: no move
    } else if (pm.s.flags & kSqTimeWaterJump) {
      velocity.z -= float(pm.s.gravity) * seconds;
      if (velocity.z < 0) {
        pm.s.flags &= uint8_t(~kTimeFlags);
        pm.s.time = 0;
      }
      step_slide();
    } else {
      jump();
      friction();
      if (pm.water_level >= 2) {
        water_move();
      } else {
        Vec3f angles = pm.view_angles;
        if (angles.x > 180) angles.x -= 360;
        angles.x /= 3;
        const ViewAxes a = angle_vectors(angles);
        forward = a.forward;
        right = a.right;
        up = a.up;
        air_move();
      }
    }
    categorise();
    snap();
  }
};

SequelTrace brush_trace(void* ctx, Vec3f start, Vec3f mins, Vec3f maxs, Vec3f end) {
  auto* w = static_cast<SequelBrushWorld*>(ctx);
  const float a[3] = {start.x, start.y, start.z}, b[3] = {end.x, end.y, end.z};
  const float lo[3] = {mins.x, mins.y, mins.z}, hi[3] = {maxs.x, maxs.y, maxs.z};
  const brush::Trace t = w->tracer->trace(a, b, lo, hi, w->mask);
  SequelTrace r;
  r.all_solid = t.all_solid;
  r.start_solid = t.start_solid;
  r.fraction = t.fraction;
  r.end = {t.end[0], t.end[1], t.end[2]};
  r.normal = {t.plane.n[0], t.plane.n[1], t.plane.n[2]};
  r.surface_flags = t.flags;
  r.contents = t.contents;
  r.entity = 0;  // the world, hit or not
  return r;
}

int32_t brush_contents(void* ctx, Vec3f p) {
  const float q[3] = {p.x, p.y, p.z};
  return static_cast<SequelBrushWorld*>(ctx)->tracer->contents(q);
}

}  // namespace

SequelCmd sequel_command(const MoveCmd& c) {
  const auto angle = [](float degrees) {
    return static_cast<int16_t>(core::num::to_int32(double(degrees) * (65536.0 / 360.0)));
  };
  SequelCmd out;
  out.msec = c.msec;
  out.buttons = c.buttons;
  out.angles[0] = angle(c.angles.x);
  out.angles[1] = angle(c.angles.y);
  out.angles[2] = angle(c.angles.z);
  out.forward = c.forward;
  out.side = c.side;
  out.up = c.up;
  if ((c.buttons & kButtonJump) != 0 && out.up < kSqJumpUp) out.up = kSqJumpUp;
  out.impulse = c.impulse;
  return out;
}

void sequel_move(SequelMove& pm, const SequelWorld& world) {
  Move m{pm, world};
  m.run();
}

SequelWorld SequelBrushWorld::world() { return {this, brush_trace, brush_contents}; }

}  // namespace ge::shooter
