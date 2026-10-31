// The original player move (framemove.hpp states the rules and the arithmetic).
//
// ge:summary The original player move's think, program part, walk move, unstick and water level
// ge:keywords frame_think frame_move frame_program_move frame_water_level user-friction
// ge:keywords accelerate air-accelerate swim water-jump try-unstick wall-friction check-stuck
// ge:skill shooter
// ge:test tests/shooter_framemove_test.cpp
#include "shooter/framemove.hpp"

#include <cmath>

#include "shooter/physics.hpp"
#include "shooter/trace.hpp"

namespace ge::shooter {

namespace {

constexpr double kFloorZ = 0.7;
constexpr float kAirWish = 30;       // the air's wish speed for the gain, units/s
constexpr float kEdgeAhead = 16;     // where the edge friction looks for the floor, units
constexpr float kEdgeDrop = 34;      // how far down it looks
constexpr float kSwimDrift = 60;     // a swimmer's sink with no input, units/s
constexpr double kSwimShare = 0.7;   // the swim speed's share of the wish
constexpr float kPunchDecay = 10;    // degrees a second
constexpr float kNoMove = 0.03125f;  // a step up that moved less than this on x and y is stuck
constexpr float kUnstuck = 4;        // an unstick nudge that gets this far wins
constexpr float kWaterDrag = 0.8f;   // the program's water drag a level a second
constexpr float kJumpSpeed = 270, kWaterJumpSpeed = 225, kWaterJumpPush = -50;

Edict& ent(Server& sv, int e) { return sv.edicts[static_cast<size_t>(e)]; }

// A float sum of squares' root in double, rounded to float.
float root(float squares) { return static_cast<float>(std::sqrt(static_cast<double>(squares))); }

void friction(Server& sv, int e, const FrameMoveSettings& set, double dt) {
  Edict& ed = ent(sv, e);
  Vec3f& v = ed.velocity;
  const float speed = root(v.x * v.x + v.y * v.y);
  if (speed == 0) return;
  Vec3f start{ed.origin.x + v.x / speed * kEdgeAhead, ed.origin.y + v.y / speed * kEdgeAhead,
              ed.origin.z + ed.mins.z};
  Vec3f stop = start;
  stop.z = start.z - kEdgeDrop;
  const HullTrace tr = trace_move(sv, start, {}, {}, stop, kTraceNoMonsters, e);
  const float f = tr.fraction == 1 ? set.friction * set.edgefriction : set.friction;
  const float control = speed < set.stopspeed ? set.stopspeed : speed;
  auto news = static_cast<float>(speed - dt * control * f);
  if (news < 0) news = 0;
  news /= speed;
  v = v * news;
}

void accelerate(Edict& ed, Vec3f dir, float wish, const FrameMoveSettings& set, double dt) {
  const float current = dot(ed.velocity, dir);
  const float add = wish - current;
  if (add <= 0) return;
  auto step = static_cast<float>(set.accelerate * dt * wish);
  if (step > add) step = add;
  ed.velocity = mad(ed.velocity, step, dir);
}

void air_accelerate(Edict& ed, Vec3f wishvel, float wish, const FrameMoveSettings& set, double dt) {
  Vec3f dir = wishvel;
  float capped = normalize(dir);
  if (capped > kAirWish) capped = kAirWish;
  const float current = dot(ed.velocity, dir);
  const float add = capped - current;
  if (add <= 0) return;
  auto step = static_cast<float>(set.accelerate * wish * dt);
  if (step > add) step = add;
  ed.velocity = mad(ed.velocity, step, dir);
}

void swim(Edict& ed, const FrameCommand& cmd, const FrameMoveSettings& set, double dt) {
  const ViewAxes ax = angle_vectors(ed.v_angle);
  Vec3f wishvel{ax.forward.x * cmd.forward + ax.right.x * cmd.side,
                ax.forward.y * cmd.forward + ax.right.y * cmd.side,
                ax.forward.z * cmd.forward + ax.right.z * cmd.side};
  if (cmd.forward == 0 && cmd.side == 0 && cmd.up == 0)
    wishvel.z -= kSwimDrift;
  else
    wishvel.z += cmd.up;
  float wish = length(wishvel);
  if (wish > set.maxspeed) {
    wishvel = wishvel * (set.maxspeed / wish);
    wish = set.maxspeed;
  }
  wish = static_cast<float>(wish * kSwimShare);
  // Friction, then the acceleration's room measured against the speed left after it (not the
  // speed along the wish: the swim_up_and_jump vectors hold it).
  const float speed = length(ed.velocity);
  float news = 0;
  if (speed != 0) {
    news = static_cast<float>(speed - dt * speed * set.friction);
    if (news < 0) news = 0;
    ed.velocity = ed.velocity * (news / speed);
  }
  if (wish == 0) return;
  const float add = wish - news;
  if (add <= 0) return;
  normalize(wishvel);
  auto step = static_cast<float>(set.accelerate * wish * dt);
  if (step > add) step = add;
  ed.velocity = mad(ed.velocity, step, wishvel);
}

float body_roll(const Edict& ed, const FrameMoveSettings& set) {
  const ViewAxes ax = angle_vectors(ed.angles);
  float side = dot(ed.velocity, ax.right);
  const float sign = side < 0 ? -1.0f : 1.0f;
  side = std::fabs(side);
  side = side < set.rollspeed ? side * set.rollangle / set.rollspeed : set.rollangle;
  return side * sign;
}

// Clear of solids at the box's spot: back to the last free spot, then a nudge.
void check_stuck(Server& sv, int e) {
  Edict& ed = ent(sv, e);
  if (!test_entity_position(sv, e)) {
    ed.oldorigin = ed.origin;
    return;
  }
  const Vec3f org = ed.origin;
  ed.origin = ed.oldorigin;
  if (!test_entity_position(sv, e)) {
    link_edict(sv, e, true);
    return;
  }
  for (int z = 0; z < static_cast<int>(kStepHeight); z++)
    for (int i = -1; i <= 1; i++)
      for (int j = -1; j <= 1; j++) {
        ed.origin = {org.x + static_cast<float>(i), org.y + static_cast<float>(j),
                     org.z + static_cast<float>(z)};
        if (!test_entity_position(sv, e)) {
          link_edict(sv, e, true);
          return;
        }
      }
  ed.origin = org;
}

int try_unstick(Server& sv, int e, Vec3f oldvel, HullTrace& wall) {
  static constexpr float kDirs[8][2] = {{2, 0}, {0, 2},  {-2, 0}, {0, -2},
                                        {2, 2}, {-2, 2}, {2, -2}, {-2, -2}};
  const Vec3f oldorg = ent(sv, e).origin;
  for (const auto& d : kDirs) {
    push_entity(sv, e, {d[0], d[1], 0});
    Edict& ed = ent(sv, e);
    ed.velocity = {oldvel.x, oldvel.y, 0};
    const int clip = fly_move(sv, e, 0.1f, &wall);
    const Edict& now = ent(sv, e);
    if (std::fabs(oldorg.y - now.origin.y) > kUnstuck ||
        std::fabs(oldorg.x - now.origin.x) > kUnstuck)
      return clip;
    ent(sv, e).origin = oldorg;
  }
  ent(sv, e).velocity = {};
  return 7;
}

void wall_friction(Edict& ed, const HullTrace& wall) {
  const ViewAxes ax = angle_vectors(ed.v_angle);
  auto d = static_cast<float>(dot(wall.plane.normal, ax.forward) + 0.5);
  if (d >= 0) return;
  const float i = dot(wall.plane.normal, ed.velocity);
  const Vec3f into = wall.plane.normal * i;
  const Vec3f side = ed.velocity - into;
  ed.velocity.x = side.x * (1 + d);
  ed.velocity.y = side.y * (1 + d);
}

int walk_move(Server& sv, int e, const FrameMoveSettings& set, double dt) {
  Edict& ed = ent(sv, e);
  const bool was_on_ground = (ed.flags & kFlagOnGround) != 0;
  ed.flags &= ~kFlagOnGround;
  const Vec3f oldorg = ed.origin;
  const Vec3f oldvel = ed.velocity;
  HullTrace wall;
  const int first = fly_move(sv, e, static_cast<float>(dt), &wall);
  if ((first & 2) == 0) return first;
  {
    const Edict& now = ent(sv, e);
    if (!was_on_ground && now.waterlevel == 0) return first;
    if (now.movetype != kMovetypeWalk || set.nostep || (now.flags & kFlagWaterJump) != 0)
      return first;
  }
  const Vec3f nostep_org = ent(sv, e).origin;
  const Vec3f nostep_vel = ent(sv, e).velocity;
  ent(sv, e).origin = oldorg;
  push_entity(sv, e, {0, 0, kStepHeight});
  ent(sv, e).velocity = {oldvel.x, oldvel.y, 0};
  int clip = fly_move(sv, e, static_cast<float>(dt), &wall);
  if (clip != 0) {
    const Edict& now = ent(sv, e);
    if (std::fabs(oldorg.y - now.origin.y) < kNoMove &&
        std::fabs(oldorg.x - now.origin.x) < kNoMove)
      clip = try_unstick(sv, e, oldvel, wall);
    if ((clip & 2) != 0) wall_friction(ent(sv, e), wall);
  }
  const auto down = static_cast<float>(-kStepHeight + oldvel.z * dt);
  const HullTrace tr = push_entity(sv, e, {0, 0, down});
  Edict& after = ent(sv, e);
  if (tr.plane.normal.z > kFloorZ) {
    if (tr.ent >= 0 && ent(sv, tr.ent).solid == kSolidBsp) {
      after.flags |= kFlagOnGround;
      after.groundentity = tr.ent;
    }
  } else {
    after.origin = nostep_org;
    after.velocity = nostep_vel;
  }
  return clip | 256;
}

}  // namespace

void frame_think(Server& sv, int e, const FrameCommand& cmd, const FrameMoveSettings& set,
                 double dt) {
  Edict& ed = ent(sv, e);
  if (ed.movetype == kMovetypeNone) return;
  const bool on_ground = (ed.flags & kFlagOnGround) != 0;
  {
    Vec3f p = ed.punchangle;
    float len = normalize(p);
    len = static_cast<float>(len - kPunchDecay * dt);
    if (len < 0) len = 0;
    ed.punchangle = p * len;
  }
  if (ed.health <= 0) return;
  ed.v_angle = cmd.view;
  const Vec3f aim = ed.v_angle + ed.punchangle;
  ed.angles.z = body_roll(ed, set) * 4;
  if (!ed.fixangle) {
    ed.angles.x = -aim.x / 3;
    ed.angles.y = aim.y;
  }
  if ((ed.flags & kFlagWaterJump) != 0) {
    if (sv.time > ed.teleport_time || ed.waterlevel == 0) {
      ed.flags &= ~kFlagWaterJump;
      ed.teleport_time = 0;
    }
    ed.velocity.x = ed.movedir.x;
    ed.velocity.y = ed.movedir.y;
    return;
  }
  if (ed.waterlevel >= 2 && ed.movetype != kMovetypeNoclip) {
    swim(ed, cmd, set, dt);
    return;
  }
  const ViewAxes ax = angle_vectors(ed.angles);
  float fmove = cmd.forward;
  const float smove = cmd.side;
  if (ed.teleport_time > sv.time && fmove < 0) fmove = 0;
  Vec3f wishvel{ax.forward.x * fmove + ax.right.x * smove,
                ax.forward.y * fmove + ax.right.y * smove,
                ax.forward.z * fmove + ax.right.z * smove};
  wishvel.z = ed.movetype != kMovetypeWalk ? cmd.up : 0.0f;
  Vec3f dir = wishvel;
  float wish = normalize(dir);
  if (wish > set.maxspeed) {
    wishvel = wishvel * (set.maxspeed / wish);
    wish = set.maxspeed;
  }
  if (ed.movetype == kMovetypeNoclip) {
    ed.velocity = wishvel;
  } else if (on_ground) {
    friction(sv, e, set, dt);
    accelerate(ent(sv, e), dir, wish, set, dt);
  } else {
    air_accelerate(ed, wishvel, wish, set, dt);
  }
}

void frame_program_move(Server& sv, int e, bool jump, double dt) {
  Edict& ed = ent(sv, e);
  if (ed.movetype != kMovetypeNoclip && ed.health > 0 && (ed.flags & kFlagWaterJump) == 0 &&
      ed.waterlevel > 0) {
    const float f = kWaterDrag * static_cast<float>(ed.waterlevel) * static_cast<float>(dt);
    ed.velocity = ed.velocity - ed.velocity * f;
  }
  if (ed.waterlevel == 2) {
    ViewAxes ax = angle_vectors(ed.angles);
    Vec3f start = ed.origin;
    start.z += 8;
    ax.forward.z = 0;
    Vec3f end = mad(start, 24, ax.forward);
    const HullTrace low = trace_move(sv, start, {}, {}, end, kTraceNoMonsters, e);
    if (low.fraction < 1) {
      start.z = start.z + ed.maxs.z - 8;
      end = mad(start, 24, ax.forward);
      ed.movedir = low.plane.normal * kWaterJumpPush;
      const HullTrace high = trace_move(sv, start, {}, {}, end, kTraceNoMonsters, e);
      if (high.fraction == 1) {
        ed.flags |= kFlagWaterJump;
        ed.velocity.z = kWaterJumpSpeed;
        ed.flags &= ~kFlagJumpReleased;
        ed.teleport_time = static_cast<float>(sv.time) + 2;
      }
    }
  }
  if (!jump) {
    ed.flags |= kFlagJumpReleased;
    return;
  }
  if ((ed.flags & kFlagWaterJump) != 0) return;
  if (ed.waterlevel >= 2) {
    ed.velocity.z = ed.watertype == kContentsWater   ? 100.0f
                    : ed.watertype == kContentsSlime ? 80.0f
                                                     : 50.0f;
    return;
  }
  if ((ed.flags & kFlagOnGround) == 0 || (ed.flags & kFlagJumpReleased) == 0) return;
  ed.flags &= ~(kFlagJumpReleased | kFlagOnGround);
  ed.velocity.z = ed.velocity.z + kJumpSpeed;
}

void frame_water_level(Server& sv, int e) {
  Edict& ed = ent(sv, e);
  Vec3f p = ed.origin;
  p.z = ed.origin.z + ed.mins.z + 1;
  ed.waterlevel = 0;
  ed.watertype = kContentsEmpty;
  int c = world_contents(sv, p);
  if (c > kContentsWater) return;
  ed.watertype = c;
  ed.waterlevel = 1;
  p.z = static_cast<float>(ed.origin.z + (ed.mins.z + ed.maxs.z) * 0.5);
  c = world_contents(sv, p);
  if (c > kContentsWater) return;
  ed.waterlevel = 2;
  p.z = ed.origin.z + ed.view_ofs.z;
  c = world_contents(sv, p);
  if (c <= kContentsWater) ed.waterlevel = 3;
}

FrameMoveSettings frame_settings(const ServerSettings& set) {
  FrameMoveSettings f;
  f.friction = set.move.friction;
  f.edgefriction = set.edgefriction;
  f.stopspeed = set.move.stopspeed;
  f.accelerate = set.move.accelerate;
  f.maxspeed = set.move.maxspeed;
  f.nostep = set.nostep;
  f.rollangle = set.rollangle;
  f.rollspeed = set.rollspeed;
  return f;
}

FrameCommand frame_command(const MoveCmd& cmd) {
  FrameCommand c;
  c.view = cmd.angles;
  c.forward = static_cast<float>(cmd.forward);
  c.side = static_cast<float>(cmd.side);
  c.up = static_cast<float>(cmd.up);
  return c;
}

int frame_move(Server& sv, int e, const FrameMoveSettings& set, double dt) {
  if (ent(sv, e).movetype == kMovetypeNoclip) {
    Edict& ed = ent(sv, e);
    ed.origin = mad(ed.origin, static_cast<float>(dt), ed.velocity);
    link_edict(sv, e, false);
    return 0;
  }
  if (ent(sv, e).movetype != kMovetypeWalk) return 0;
  frame_water_level(sv, e);
  Edict& ed = ent(sv, e);
  if (ed.waterlevel < 2 && (ed.flags & kFlagWaterJump) == 0) {
    const float factor = ed.gravity != 0 ? ed.gravity : 1.0f;
    ed.velocity.z = static_cast<float>(ed.velocity.z - factor * sv.set.move.gravity * dt);
  }
  for (int i = 0; i < 3; i++) {
    float& c = ed.velocity[i];
    if (c != c) c = 0;
    if (c > sv.set.maxvelocity) c = sv.set.maxvelocity;
    if (c < -sv.set.maxvelocity) c = -sv.set.maxvelocity;
  }
  check_stuck(sv, e);
  const int clip = walk_move(sv, e, set, dt);
  link_edict(sv, e, true);
  return clip;
}

}  // namespace ge::shooter
