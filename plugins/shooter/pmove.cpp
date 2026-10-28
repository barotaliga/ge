// The classic move's steps in their order (pmove.hpp lists the order; each step's rules are
// stated beside it here). The slide and the step are slide.cpp's.
//
// ge:summary The classic move's steps: unstick, categorise, jumps, friction, the wish, the moves
// ge:keywords player_move run_command nudge unstick categorize water-level friction edge-friction
// ge:keywords accelerate air_accelerate jump water-jump spectator walk-move water-move fly-move
// ge:skill shooter move
// ge:test tests/shooter_pmove_test.cpp tests/shooter_recording_test.cpp
#include "shooter/pmove.hpp"

#include "shooter/brush_hull.hpp"
#include "shooter/slide.hpp"

namespace ge::shooter {

namespace {

constexpr double kFloorZ = 0.7;           // a plane whose normal z reaches this is ground
constexpr float kGroundSpeedLimit = 180;  // rising faster than this is never on the ground
constexpr float kJumpSpeed = 270;
constexpr float kAirWishCap = 30;  // the air's wish speed cap in the gain's subtraction
constexpr float kEdgeAhead = 16;   // the edge probe: this far ahead of the origin
constexpr float kEdgeDepth = 34;   // and this far down from the feet
constexpr float kWaterJumpUp = 310;
constexpr float kWaterJumpOut = 50;
constexpr float kWaterJumpTime = 2;  // s
constexpr float kIdleSink = 60;      // units/s down when swimming with no input
constexpr float kLandingSpeed = 300;

bool flying(const MoveState& s) { return s.type == MoveType::fly; }

// GROUND, WATER, SNAP (pmove.hpp step 6).
void categorize(Mover& m) {
  MoveState& s = m.s;
  HullTrace tr;
  bool traced = false;
  if (s.velocity.z > kGroundSpeedLimit) {
    s.ground = -1;
  } else if (!(m.set.pground && s.ground == -1)) {
    tr = player_trace(m.world, s.origin, {s.origin.x, s.origin.y, s.origin.z - 1});
    traced = true;
    if (tr.fraction < 1 && tr.plane.normal.z >= kFloorZ) {
      s.ground = tr.ent;
      s.ground_normal = tr.plane.normal;
      s.waterjump_time = 0;
      if (tr.ent > 0) m.touches.add(tr.ent);
    } else {
      s.ground = -1;
    }
  }

  // The water level: the feet + 1, the box's middle, the eyes.
  s.waterlevel = 0;
  s.watertype = kContentsEmpty;
  Vec3f p = {s.origin.x, s.origin.y, s.origin.z + (kPlayerMins.z + 1)};
  int c = water_contents(m.world, p);
  if (c <= kContentsWater) {
    s.watertype = c;
    s.waterlevel = 1;
    p.z = s.origin.z + (kPlayerMins.z + kPlayerMaxs.z) * 0.5f;
    c = water_contents(m.world, p);
    if (c <= kContentsWater) {
      s.waterlevel = 2;
      p.z = s.origin.z + 22;
      c = water_contents(m.world, p);
      if (c <= kContentsWater) s.waterlevel = 3;
    }
  }

  if (!m.set.pground && s.ground != -1 && !flying(s) && s.waterlevel < 2 && traced &&
      !tr.startsolid && !tr.allsolid)
    s.origin = tr.endpos;
}

// UNSTICK (pmove.hpp step 5): a client's copy of a position sits on a grid of 1/8 unit, so it may
// be a hair inside a wall; a map may sink a spawn into the floor.
void nudge(Mover& m) {
  MoveState& s = m.s;
  const Vec3f base = s.origin;
  constexpr float kSign[3] = {0, -1, 1};
  for (int z = 0; z < 3; z++)
    for (int y = 0; y < 3; y++)
      for (int x = 0; x < 3; x++) {
        const Vec3f p = {base.x + kSign[x] * 0.125f, base.y + kSign[y] * 0.125f,
                         base.z + kSign[z] * 0.125f};
        if (box_free(m.world, p)) {
          s.origin = p;
          return;
        }
      }
  for (int i = 1; i <= 18; i++) {
    const Vec3f p = {base.x, base.y, base.z + static_cast<float>(i)};
    if (box_free(m.world, p)) {
      s.origin = p;
      return;
    }
  }
}

// THE WATER JUMP (step 7): at water level 2, facing a bank whose lip is solid 8 up and open 32
// up, the player is thrown up and out.
void check_water_jump(Mover& m, const ViewAxes& ax) {
  MoveState& s = m.s;
  if (s.waterjump_time > 0) return;
  if (s.velocity.z < -kGroundSpeedLimit) return;
  Vec3f flat = ax.forward;
  flat.z = 0;
  normalize(flat);
  Vec3f spot = {s.origin.x + flat.x * 24, s.origin.y + flat.y * 24, s.origin.z + 8};
  if (brush_contents(m.world, spot) != kContentsSolid) return;
  spot.z = spot.z + 24;
  if (brush_contents(m.world, spot) != kContentsEmpty) return;
  s.velocity = {flat.x * kWaterJumpOut, flat.y * kWaterJumpOut, kWaterJumpUp};
  s.waterjump_time = kWaterJumpTime;
  s.jump_held = true;
}

// THE JUMP (step 9): the first rule that applies decides.
void jump(Mover& m, const MoveCmd& cmd) {
  MoveState& s = m.s;
  if (flying(s)) return;
  if (s.type == MoveType::dead) {
    s.jump_held = true;  // so a respawn does not jump
    return;
  }
  if ((cmd.buttons & kButtonJump) == 0) {
    s.jump_held = false;
    return;
  }
  if (s.waterjump_time > 0) return;
  if (s.waterlevel >= 2) {  // swimming up, not jumping
    s.ground = -1;
    s.velocity.z = s.watertype == kContentsWater ? 100 : s.watertype == kContentsSlime ? 80 : 50;
    return;
  }
  if (s.ground == -1) return;
  if (s.jump_held) return;  // no pogo: the button must be let go between jumps

  if (dot(s.velocity, s.ground_normal) < -0.1 && s.velocity.z < 0)
    s.velocity = clip_velocity(s.velocity, s.ground_normal, 1);
  s.ground = -1;
  s.velocity.z = s.velocity.z + kJumpSpeed;
  if (m.set.ktjump > 0) {
    const float k = m.set.ktjump > 1 ? 1 : m.set.ktjump;
    if (s.velocity.z < kJumpSpeed) s.velocity.z = s.velocity.z * (1 - k) + kJumpSpeed * k;
  }
  s.jump_held = true;
}

// FRICTION (step 10): water, flying or the ground (doubled where nothing is under the leading
// edge); none in the air.
void friction(Mover& m) {
  MoveState& s = m.s;
  if (s.waterjump_time > 0) return;
  const float speed = length(s.velocity);
  if (speed < 1) {
    s.velocity.x = 0;
    s.velocity.y = 0;
    if (flying(s)) s.velocity.z = 0;
    return;
  }
  float drop = 0;
  if (s.waterlevel >= 2) {
    drop = speed * m.set.waterfriction * static_cast<float>(s.waterlevel) * m.dt;
  } else if (flying(s)) {
    drop = speed * 4 * m.dt;
  } else if (s.ground != -1) {
    float fric = m.set.friction;
    const Vec3f start = {s.origin.x + s.velocity.x / speed * kEdgeAhead,
                         s.origin.y + s.velocity.y / speed * kEdgeAhead,
                         s.origin.z + kPlayerMins.z};
    const HullTrace tr = player_trace(m.world, start, {start.x, start.y, start.z - kEdgeDepth});
    if (tr.fraction == 1) fric = fric * 2;
    const float control = speed < m.set.stopspeed ? m.set.stopspeed : speed;
    drop = control * fric * m.dt;
  }
  float newspeed = speed - drop;
  if (newspeed < 0) newspeed = 0;
  newspeed = newspeed / speed;
  s.velocity = s.velocity * newspeed;
}

bool accelerates(const MoveState& s) { return s.type != MoveType::dead && s.waterjump_time <= 0; }

// On the ground, in water and flying.
void accelerate(Mover& m, Vec3f wishdir, float wishspeed, float accel) {
  MoveState& s = m.s;
  if (!accelerates(s)) return;
  const float add = wishspeed - dot(s.velocity, wishdir);
  if (add <= 0) return;
  float speed = accel * m.dt * wishspeed;
  if (speed > add) speed = add;
  s.velocity = mad(s.velocity, speed, wishdir);
}

float horizontal_speed(Vec3f v) { return root_of(v.x * v.x + v.y * v.y); }

// In the air: the wish capped at 30 in the subtraction only, so turning keeps the gain coming.
void air_accelerate(Mover& m, Vec3f wishdir, float wishspeed, float accel) {
  MoveState& s = m.s;
  if (!accelerates(s)) return;
  const float wishspd = wishspeed > kAirWishCap ? kAirWishCap : wishspeed;
  const float add = wishspd - dot(s.velocity, wishdir);
  if (add <= 0) return;
  float speed = accel * wishspeed * m.dt;
  if (speed > add) speed = add;
  const float h0 = horizontal_speed(s.velocity);
  s.velocity = mad(s.velocity, speed, wishdir);
  if (m.set.bunnyspeedcap > 0) {
    const float cap = s.maxspeed * m.set.bunnyspeedcap;
    const float h1 = horizontal_speed(s.velocity);
    if (h1 > h0 && h1 > cap) {
      const float scale = (h0 > cap ? h0 : cap) / h1;
      s.velocity.x = s.velocity.x * scale;
      s.velocity.y = s.velocity.y * scale;
    }
  }
}

// The wish's direction and speed off a wish velocity, capped at `cap`.
float wish_of(Vec3f wishvel, float cap, Vec3f& wishdir) {
  wishdir = wishvel;
  float wishspeed = normalize(wishdir);
  if (wishspeed > cap) wishspeed = cap;
  return wishspeed;
}

Vec3f wish_velocity(const ViewAxes& ax, const MoveCmd& cmd, bool flatten) {
  Vec3f f = ax.forward, r = ax.right;
  if (flatten) {
    f.z = 0;
    r.z = 0;
    normalize(f);
    normalize(r);
  }
  const auto fmove = static_cast<float>(cmd.forward);
  const auto smove = static_cast<float>(cmd.side);
  Vec3f w;
  for (int i = 0; i < 3; i++) w[i] = f[i] * fmove + r[i] * smove;
  if (flatten) w.z = 0;
  return w;
}

int walk_move(Mover& m, const MoveCmd& cmd, const ViewAxes& ax) {
  MoveState& s = m.s;
  Vec3f wishdir;
  const float wishspeed = wish_of(wish_velocity(ax, cmd, true), s.maxspeed, wishdir);
  const float fall = s.entgravity * m.set.gravity * m.dt;

  if (s.ground != -1) {
    // Without the slide fix the ground is flat to the move; with it a downward speed is kept.
    if (!m.set.slidefix || s.velocity.z > 0) s.velocity.z = 0;
    accelerate(m, wishdir, wishspeed, m.set.accelerate);
    if (m.set.slidefix) s.velocity.z = s.velocity.z - fall;
    if (s.velocity.x == 0 && s.velocity.y == 0) {
      s.velocity.z = 0;
      return 0;
    }
    return step_slide_move(m, false);
  }

  air_accelerate(m, wishdir, wishspeed, m.set.accelerate);
  s.velocity.z = s.velocity.z - fall;
  const int blocked = m.set.airstep ? step_slide_move(m, true) : slide_move(m);
  if (m.set.pground && (blocked & kBlockedFloor) != 0) {
    s.ground = m.floor_ent;
    s.ground_normal = m.floor_normal;
  }
  return blocked;
}

// Swimming (water) and flying: the full view, the up axis on z; no gravity.
int swim_or_fly_move(Mover& m, const MoveCmd& cmd, const ViewAxes& ax, bool water) {
  MoveState& s = m.s;
  Vec3f wishvel = wish_velocity(ax, cmd, false);
  if (water && !flying(s) && cmd.forward == 0 && cmd.side == 0 && cmd.up == 0)
    wishvel.z = wishvel.z - kIdleSink;
  else
    wishvel.z = wishvel.z + static_cast<float>(cmd.up);
  Vec3f wishdir;
  float wishspeed = wish_of(wishvel, s.maxspeed, wishdir);
  if (water) wishspeed = static_cast<float>(wishspeed * 0.7);
  accelerate(m, wishdir, wishspeed, water ? m.set.wateraccelerate : m.set.accelerate);
  return step_slide_move(m, false);
}

// SPECTATORS (step 4): their own friction, the full view, no collision.
void spectator_move(Mover& m, const MoveCmd& cmd, const ViewAxes& ax) {
  MoveState& s = m.s;
  const float speed = length(s.velocity);
  if (speed < 1) {
    s.velocity = {};
  } else {
    const float control = speed < m.set.stopspeed ? m.set.stopspeed : speed;
    const float drop = control * m.set.friction * 1.5f * m.dt;
    float newspeed = speed - drop;
    if (newspeed < 0) newspeed = 0;
    newspeed = newspeed / speed;
    s.velocity = s.velocity * newspeed;
  }

  ViewAxes n = ax;
  normalize(n.forward);
  normalize(n.right);
  normalize(n.up);
  Vec3f wishvel = wish_velocity(n, cmd, false);
  wishvel.z = wishvel.z + static_cast<float>(cmd.up);
  Vec3f wishdir;
  const float wishspeed = wish_of(wishvel, m.set.spectatormaxspeed, wishdir);

  const float add = wishspeed - dot(s.velocity, wishdir);
  if (s.type == MoveType::old_spectator && add <= 0) return;  // coasting faster: no move at all
  if (add > 0) {
    float accel = m.set.accelerate * m.dt * wishspeed;
    if (accel > add) accel = add;
    s.velocity = mad(s.velocity, accel, wishdir);
  }
  s.origin = mad(s.origin, m.dt, s.velocity);
}

}  // namespace

MoveResult player_move(MoveState& s, const MoveCmd& cmd, const MoveSettings& set,
                       const MoveWorld& world) {
  MoveResult r;
  Mover m{s, set, world, r.touches};
  m.dt = static_cast<float>(cmd.msec * 0.001);

  if (s.type == MoveType::none || s.type == MoveType::lock) {
    categorize(m);
    return r;
  }

  s.angles = cmd.angles;
  const ViewAxes ax = angle_vectors(s.angles);

  if (s.type == MoveType::spectator || s.type == MoveType::old_spectator) {
    spectator_move(m, cmd, ax);
    s.ground = -1;
    return r;
  }

  nudge(m);
  categorize(m);

  if (s.waterlevel == 2 && !flying(s)) check_water_jump(m, ax);

  if (s.velocity.z < 0 || s.type == MoveType::dead) {
    s.waterjump_time = 0;
  } else if (s.waterjump_time > 0) {
    s.waterjump_time = s.waterjump_time - m.dt;
    if (s.waterjump_time < 0) s.waterjump_time = 0;
  }

  jump(m, cmd);
  friction(m);

  if (s.waterlevel >= 2)
    r.blocked = swim_or_fly_move(m, cmd, ax, true);
  else if (flying(s))
    r.blocked = swim_or_fly_move(m, cmd, ax, false);
  else
    r.blocked = walk_move(m, cmd, ax);

  categorize(m);

  // THE LANDING FIX: a hard landing into the ground plane is clipped to it.
  if (!set.pground && s.ground != -1 && s.velocity.z < -kLandingSpeed &&
      dot(s.velocity, s.ground_normal) < -0.1)
    s.velocity = clip_velocity(s.velocity, s.ground_normal, 1);
  return r;
}

MoveResult run_command(MoveState& s, const MoveCmd& cmd, const MoveSettings& set,
                       const MoveWorld& world) {
  if (cmd.msec <= 50) return player_move(s, cmd, set, world);
  MoveCmd half = cmd;
  half.msec = static_cast<uint8_t>(cmd.msec / 2);
  MoveResult first = run_command(s, half, set, world);
  half.impulse = 0;
  MoveResult r = run_command(s, half, set, world);
  for (int i = 0; i < r.touches.count; i++) first.touches.add(r.touches.ents[i]);
  r.touches = first.touches;
  return r;
}

}  // namespace ge::shooter
