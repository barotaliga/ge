// The step's body: the crouch, the jump, friction, the acceleration rule, the slide along the
// planes a move hits, the step up, the ground's probe and snap, and the events.
//
// ge:summary The step's body: crouch, jump, friction, accelerate, slide move, step up, ground
// ge:keywords move step slide-move clip planes crease bumps step-up categorize ground probe
// ge:keywords edge friction coyote buffer jump-cut air-control air-strafe air-stop
// ge:skill move
// ge:test tests/move_step_test.cpp
#include "move/step.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::move {

namespace {

constexpr int kBumps = 4;         // planes a move may slide along in a step
constexpr double kNever = 1e300;  // `since_ground` after a jump: no coyote jump from the air

Vec3 clip(const Vec3& v, const Vec3& n) { return add_scaled(v, n, -dot(v, n)); }

struct Slid {
  bool hit = false;      // touched anything
  bool wall = false;     // touched something too steep to stand on
  double wall_lost = 0;  // L/s of plan speed the walls took
};

// THE SLIDE MOVE: the body carried along `vel` for `dt`, each surface hit taking the part of the
// velocity into it, up to `kBumps` surfaces; two at once leave the crease between them, three
// stop it. A surface steeper than walkable counts as a wall.
Slid slide_move(const MoveSpec& spec, const World& world, const Box& box, Vec3& pos, Vec3& vel,
                double dt) {
  Slid out;
  const Vec3 primal = vel;
  Vec3 planes[kBumps + 1];
  int np = 0;
  double left = dt;
  for (int bump = 0; bump < kBumps; bump++) {
    if (vel.x == 0 && vel.y == 0 && vel.z == 0) break;
    const Hit h = world.sweep(box, pos, add_scaled(pos, vel, left));
    if (h.all_solid) {  // inside something: stand still and let the game sort it out
      vel = {0, 0, 0};
      break;
    }
    if (h.fraction > 0) {
      pos = h.end;
      np = 0;
    }
    if (h.fraction == 1) break;
    out.hit = true;
    if (h.normal.y < spec.walk_normal) out.wall = true;
    left -= left * h.fraction;
    planes[np++] = h.normal;
    const Vec3 before = vel;
    int i = 0;
    for (; i < np; i++) {
      vel = clip(before, planes[i]);
      int j = 0;
      for (; j < np; j++) {
        if (j != i && dot(vel, planes[j]) < 0) break;
      }
      if (j == np) break;
    }
    if (i == np) {  // no one plane's clip clears the others: follow the crease, or stop
      if (np != 2) {
        vel = {0, 0, 0};
        break;
      }
      const Vec3 dir = unit(cross(planes[0], planes[1]));
      vel = scale(dir, dot(dir, before));
    }
    if (dot(vel, primal) <= 0) {  // turned back on itself: stop rather than jitter in a corner
      vel = {0, 0, 0};
      break;
    }
    if (np == kBumps) break;
  }
  if (out.wall) {
    const double lost = flat_length(primal) - flat_length(vel);
    out.wall_lost = lost > 0 ? lost : 0;
  }
  return out;
}

bool walkable(const MoveSpec& spec, const Hit& h) {
  return h.touched() && !h.all_solid && h.normal.y >= spec.walk_normal;
}

bool fits(const World& world, const Box& box, const Vec3& pos) {
  return !world.sweep(box, pos, pos).start_solid;
}

// THE STEP UP: a move that touched anything tried again lifted by a step, carried, and set down;
// the lifted try is kept when it got farther over the plan and stands on walkable ground. Over a
// step's riser that is climbing it; up a ramp it is what keeps the plan speed (the plain move
// would lose the part of it the ramp's face takes every step), and is not reported as a step.
Slid ground_move(const MoveSpec& spec, const World& world, const Box& box, MoveState& s, double dt,
                 MoveEvents& ev) {
  const Vec3 p0 = s.pos, v0 = s.vel;
  Slid down = slide_move(spec, world, box, s.pos, s.vel, dt);
  if (!down.hit || spec.step_height <= 0) return down;
  const Vec3 pd = s.pos, vd = s.vel;

  Vec3 pu = world.sweep(box, p0, add(p0, {0, spec.step_height, 0})).end;
  const double lifted = pu.y - p0.y;
  Vec3 vu = v0;
  const Slid up = slide_move(spec, world, box, pu, vu, dt);
  const Hit set = world.sweep(box, pu, sub(pu, {0, lifted, 0}));
  if (!walkable(spec, set)) {
    s.pos = pd;
    s.vel = vd;
    return down;
  }
  pu = set.end;
  const double far_down = flat_length(sub(pd, p0)), far_up = flat_length(sub(pu, p0));
  if (far_down >= far_up) {
    s.pos = pd;
    s.vel = vd;
    return down;
  }
  s.pos = pu;
  s.vel = {vu.x, vd.y, vu.z};
  const double climbed = pu.y - p0.y;
  if (down.wall && climbed > spec.contact) {
    ev.bits |= kSteppedUp;
    ev.stepped += climbed;
  }
  return up;
}

// Is the ground ahead of the feet (along the plan velocity) gone within a step's drop?
bool near_edge(const MoveSpec& spec, const World& world, const MoveState& s) {
  const Vec3 dir = unit(flat(s.vel));
  const Vec3 at = add_scaled(s.pos, dir, spec.edge_probe);
  const Vec3 from = add(at, {0, spec.contact, 0});
  const Box point{{0, 0, 0}, {0, 0, 0}};
  const Hit h = world.sweep(point, from, sub(at, {0, spec.step_height, 0}));
  return !h.touched() && !h.start_solid;
}

void apply_friction(const MoveSpec& spec, const World& world, MoveState& s, double factor,
                    bool edges, double dt) {
  const double speed = length(s.vel);
  if (speed <= 0) return;
  double fr = factor;
  if (edges && spec.edge_friction != 1 && near_edge(spec, world, s)) fr *= spec.edge_friction;
  const double control = speed < spec.stop_speed ? spec.stop_speed : speed;
  const double drop = control * fr * dt;
  const double next = speed - drop > 0 ? speed - drop : 0;
  s.vel = scale(s.vel, next / speed);
}

// The forward-only steering of the air: the plan velocity turned toward the wish, harder the more
// it already points there, its speed kept.
void steer_in_air(const MoveSpec& spec, Vec3& vel, const Vec3& dir, double dt) {
  const double speed = flat_length(vel);
  if (speed <= 0) return;
  Vec3 v = scale(flat(vel), 1.0 / speed);
  const double d = dot(v, dir);
  if (d <= 0) return;
  const double k = spec.air_control * d * d * dt;
  v = unit(add_scaled(scale(v, speed), dir, k));
  vel = {v.x * speed, vel.y, v.z * speed};
}

void update_crouch(const MoveSpec& spec, const World& world, MoveState& s, const MoveCmd& cmd,
                   double dt, MoveEvents& ev) {
  const double lift = spec.height - spec.crouch_height;
  if (cmd.crouch && !s.ducked) {
    if (!s.grounded && spec.crouch_jump) s.pos.y += lift;  // the feet drawn up, the head stays
    s.ducked = true;
    ev.bits |= kDucked;
  } else if (!cmd.crouch && s.ducked) {
    const Box stand = hull(spec, false);
    bool stood = false;
    if (!s.grounded && spec.crouch_jump) {
      const Vec3 lowered = sub(s.pos, {0, lift, 0});
      if (fits(world, stand, lowered)) {
        s.pos = lowered;
        stood = true;
      }
    }
    if (!stood && fits(world, stand, s.pos)) stood = true;
    if (stood) {
      s.ducked = false;
      ev.bits |= kStood;
    }
  }
  const double target = s.ducked ? 1.0 : 0.0;
  s.crouch =
      spec.crouch_time > 0 ? core::approach(s.crouch, target, dt / spec.crouch_time) : target;
}

// The ground under the feet after a move: found within `contact` (or `snap` for a walker that was
// standing), the body set onto it.
void find_ground(const MoveSpec& spec, const World& world, MoveState& s, double snap,
                 MoveEvents& ev) {
  s.grounded = false;
  if (s.vel.y > spec.unground_speed) return;
  const double probe = snap > spec.contact ? snap : spec.contact;
  const Hit h = world.sweep(hull(spec, s.ducked), s.pos, sub(s.pos, {0, probe, 0}));
  if (!walkable(spec, h)) return;
  const double drop = s.pos.y - h.end.y;
  if (drop > spec.contact) {
    ev.bits |= kSteppedDown;
    ev.stepped -= drop;
  }
  s.pos = h.end;
  s.grounded = true;
  s.ground_normal = h.normal;
}

void free_flight(const MoveSpec& spec, const World& world, MoveState& s, const MoveCmd& cmd,
                 double dt, MoveEvents& ev) {
  apply_friction(spec, world, s, spec.friction, false, dt);
  const Wish w = wish_of(spec, s, cmd);
  accelerate(s.vel, w.dir, w.speed, w.speed, spec.accelerate, dt);
  s.grounded = false;
  if (s.mode == Mode::noclip) {
    s.pos = add_scaled(s.pos, s.vel, dt);
    return;
  }
  const Slid slid = slide_move(spec, world, hull(spec, s.ducked), s.pos, s.vel, dt);
  if (slid.wall) {
    ev.bits |= kBlocked;
    ev.blocked = slid.wall_lost;
  }
}

}  // namespace

Box hull(const MoveSpec& spec, bool ducked) {
  const double h = ducked ? spec.crouch_height : spec.height;
  return Box{{-spec.radius, 0, -spec.radius}, {spec.radius, h, spec.radius}};
}

double eye_height(const MoveSpec& spec, double crouch) {
  return core::lerp(spec.eye_height, spec.crouch_eye, crouch);
}

MoveState stand_at(const MoveSpec& spec, const World& world, const Vec3& pos) {
  MoveState s;
  // Lifted by a step first, so feet put down exactly on a surface start outside it.
  s.pos = add(pos, {0, spec.step_height, 0});
  s.air_jumps_left = spec.air_jumps;
  MoveEvents ignored;
  find_ground(spec, world, s, 2 * spec.step_height, ignored);
  if (!s.grounded) s.pos = pos;
  if (!s.grounded) s.since_ground = kNever;  // put down in the air: no coyote jump
  return s;
}

void accelerate(Vec3& vel, const Vec3& wish_dir, double wish_speed, double cap, double accel,
                double dt) {
  const double add = cap - dot(vel, wish_dir);
  if (add <= 0) return;
  double a = accel * wish_speed * dt;
  if (a > add) a = add;
  vel = add_scaled(vel, wish_dir, a);
}

Wish wish_of(const MoveSpec& spec, const MoveState& s, const MoveCmd& cmd) {
  const double f = core::clamp(cmd.forward, -1, 1) * (cmd.forward < 0 ? spec.back_scale : 1.0);
  const double r = core::clamp(cmd.side, -1, 1) * spec.side_scale;
  const double sy = core::libm::sin(cmd.yaw), cy = core::libm::cos(cmd.yaw);
  const Vec3 right{cy, 0, -sy};
  Vec3 raw;
  double scale_by = 1;
  if (s.mode == Mode::walk) {
    raw = add(scale(Vec3{sy, 0, cy}, f), scale(right, r));
    if (cmd.walk) scale_by *= spec.walk_scale;
    if (cmd.sprint) scale_by *= spec.sprint_scale;
    if (s.ducked && s.grounded && !s.sliding) scale_by *= spec.crouch_scale;
  } else {
    const double sp = core::libm::sin(cmd.pitch), cp = core::libm::cos(cmd.pitch);
    raw = add(add(scale(Vec3{sy * cp, sp, cy * cp}, f), scale(right, r)),
              Vec3{0, core::clamp(cmd.up, -1, 1), 0});
    scale_by = spec.fly_scale * (cmd.walk ? spec.walk_scale : 1.0) *
               (cmd.sprint ? spec.sprint_scale : 1.0);
  }
  const double n = length(raw);
  if (n <= 0) return {};
  return {scale(raw, 1.0 / n), spec.max_speed * (n < 1 ? n : 1.0) * scale_by};
}

MoveEvents step_move(const MoveSpec& spec, const World& world, MoveState& s, const MoveCmd& cmd,
                     double dt) {
  MoveEvents ev;
  s.t += dt;
  if (s.mode != Mode::walk) {
    free_flight(spec, world, s, cmd, dt, ev);
    s.jump_held = cmd.jump;
    return ev;
  }
  const bool was_grounded = s.grounded;
  update_crouch(spec, world, s, cmd, dt, ev);

  // The slide: started by crouching fast on the ground, over when slow, standing or airborne.
  const double plan_speed = flat_length(s.vel);
  if (s.sliding &&
      (!cmd.crouch || !s.grounded || plan_speed < spec.max_speed * spec.crouch_scale)) {
    s.sliding = false;
    ev.bits |= kSlideEnd;
  } else if (!s.sliding && spec.slide_min_speed > 0 && s.grounded && cmd.crouch &&
             plan_speed >= spec.slide_min_speed) {
    s.sliding = true;
    ev.bits |= kSlideStart;
    if (s.slide_ready <= 0 && spec.slide_boost > 0) {
      s.vel = add_scaled(s.vel, unit(flat(s.vel)), spec.slide_boost);
      s.slide_ready = spec.slide_cooldown;
    }
  }
  if (s.slide_ready > 0) s.slide_ready -= dt;

  // The jump: an edge of the button (kept `jump_buffer` s), or a held button when `jump_hold`.
  const bool pressed = cmd.jump && !s.jump_held;
  if (pressed) s.buffer_left = spec.jump_buffer;
  const bool want = pressed || s.buffer_left > 0 || (spec.jump_hold && cmd.jump);
  bool jumped = false;
  if (want && (s.grounded || s.since_ground < spec.coyote_time)) {
    const double base = spec.jump_additive && s.vel.y > 0 ? s.vel.y : 0;
    s.vel.y = base + spec.jump_speed;
    s.grounded = false;
    s.sliding = false;
    s.since_ground = kNever;
    s.buffer_left = 0;
    s.jump_rising = true;
    s.air_jumps_left = spec.air_jumps;
    ev.bits |= kJumped;
    jumped = true;
  } else if (pressed && !s.grounded && s.air_jumps_left > 0) {
    s.vel.y = spec.air_jump_speed > 0 ? spec.air_jump_speed : spec.jump_speed;
    s.air_jumps_left--;
    s.buffer_left = 0;
    s.jump_rising = true;
    ev.bits |= kAirJumped;
  }
  if (s.buffer_left > 0) s.buffer_left -= dt;
  if (s.jump_rising && (!cmd.jump || s.vel.y <= 0)) {
    if (!cmd.jump && s.vel.y > 0) s.vel.y *= spec.jump_cut;
    s.jump_rising = false;
  }
  s.jump_held = cmd.jump;

  const Wish w = wish_of(spec, s, cmd);
  const Box box = hull(spec, s.ducked);
  Slid slid;
  double vy_into = 0;
  if (s.grounded) {
    s.vel.y = 0;
    apply_friction(spec, world, s, s.sliding ? spec.slide_friction : spec.friction, true, dt);
    accelerate(s.vel, w.dir, w.speed, w.speed,
               spec.accelerate * (s.sliding ? spec.slide_steer : 1.0), dt);
    slid = ground_move(spec, world, box, s, dt, ev);
  } else {
    double accel = spec.air_accelerate, wish_speed = w.speed;
    double cap =
        spec.air_speed_cap > 0 && spec.air_speed_cap < w.speed ? spec.air_speed_cap : w.speed;
    if (spec.air_stop_accelerate > 0 && dot(s.vel, w.dir) < 0) accel = spec.air_stop_accelerate;
    if (spec.air_strafe_accelerate > 0 && cmd.forward == 0 && cmd.side != 0) {
      accel = spec.air_strafe_accelerate;
      if (spec.air_strafe_cap > 0 && spec.air_strafe_cap < wish_speed) {
        wish_speed = spec.air_strafe_cap;
        cap = wish_speed;
      }
    }
    const double plan_before = flat_length(s.vel);
    accelerate(s.vel, w.dir, wish_speed, cap, accel, dt);
    if (spec.air_control > 0 && cmd.side == 0 && cmd.forward != 0 && w.speed > 0) {
      steer_in_air(spec, s.vel, w.dir, dt);
    }
    if (spec.air_max_speed > 0) {  // steering kept, speed past the limit not given
      const double most = plan_before > spec.air_max_speed ? plan_before : spec.air_max_speed;
      const double now = flat_length(s.vel);
      if (now > most) {
        const double k = most / now;
        s.vel = {s.vel.x * k, s.vel.y, s.vel.z * k};
      }
    }
    // Gravity by halves either side of the move: a jump's apex is v^2 / 2g at any step length.
    const double g = spec.gravity * (s.vel.y < 0 ? spec.fall_gravity_scale : 1.0);
    s.vel.y -= g * dt * 0.5;
    vy_into = s.vel.y;
    slid = slide_move(spec, world, box, s.pos, s.vel, dt);
    s.vel.y -= g * dt * 0.5;
    if (spec.terminal_speed > 0 && s.vel.y < -spec.terminal_speed) s.vel.y = -spec.terminal_speed;
  }
  if (slid.wall && slid.wall_lost > 0) {
    ev.bits |= kBlocked;
    ev.blocked = slid.wall_lost;
  }

  const double snap = was_grounded && !jumped ? spec.ground_snap : 0;
  find_ground(spec, world, s, snap, ev);
  if (s.grounded) {
    if (dot(s.vel, s.ground_normal) < 0) s.vel = clip(s.vel, s.ground_normal);
    if (!was_grounded) {
      ev.bits |= kLanded;
      ev.impact = vy_into < 0 ? -vy_into : 0;
      // A landing the step down already counted is a landing, not a step.
      ev.bits = static_cast<uint16_t>(ev.bits & ~kSteppedDown);
      ev.stepped = 0;
    }
    s.since_ground = 0;
    s.jump_rising = false;
    s.air_jumps_left = spec.air_jumps;
  } else {
    if (was_grounded && !jumped) {
      ev.bits |= kLeftGround;
      s.since_ground = 0;
    }
    if (s.since_ground < kNever) s.since_ground += dt;
    if (s.sliding) {
      s.sliding = false;
      ev.bits |= kSlideEnd;
    }
  }
  return ev;
}

}  // namespace ge::move
