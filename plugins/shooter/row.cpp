// The classic rows' numbers and one command through the engine's mover and swimmer (row.hpp
// states the rows and the rules this file adds).
//
// ge:summary The classic rows' numbers, the command's conversion, the swim and the water jump
// ge:keywords classic_rows row_command row_body row_state engine-cmd wish-scale ground-probe
// ge:keywords swim-up water-jump ledge immersion watertype
// ge:skill shooter move water
// ge:test tests/shooter_row_test.cpp tests/shooter_row_recording_test.cpp
#include "shooter/row.hpp"

#include "core/libm/libm.hpp"

namespace ge::shooter {

namespace {

constexpr double kDegrees = 3.14159265358979323846 / 180;
constexpr double kOriginOverFeet = 24;  // the map's origin over the player's feet, units
constexpr double kAirWishCap = 30;      // the air's wish held to this along it, units/s
constexpr double kJumpSpeed = 270;
constexpr double kLandingClip = 300;  // a landing faster than this is cut into the ground
constexpr double kEdgeFriction = 2;
constexpr double kStepHeight = 18;
constexpr double kGroundSpeedLimit = 180;
constexpr double kFlyFriction = 4;
constexpr double kSpectatorFriction = 1.5;  // the ground's friction's factor for a spectator
constexpr double kSwimShare = 0.7;
constexpr double kIdleSink = 60;  // before the swim's share
constexpr double kWaterJumpUp = 310, kWaterJumpOut = 50, kWaterJumpTime = 2;
constexpr double kLedgeAhead = 24, kLedgeLow = 32, kLedgeHigh = 56;  // over the feet

// The classic's swim-up speed on the jump button, by the liquid.
double swim_up(int watertype) {
  return watertype == kContentsWater ? 100 : watertype == kContentsSlime ? 80 : 50;
}

// The ground under the feet within the row's contact, without moving the body (the swim's
// probe: in the water nothing snaps a body down).
void probe_ground(RowBody& r, const move::MoveSpec& spec, const move::World& world, bool snap) {
  move::MoveState& s = r.body;
  s.grounded = false;
  s.ground_solid = 0;
  if (s.vel.y > spec.unground_speed) return;
  const move::Hit h =
      world.sweep(move::hull(spec, false), s.pos, move::sub(s.pos, {0, spec.contact, 0}));
  if (!h.touched() || h.all_solid || h.normal.y < spec.walk_normal) return;
  s.grounded = true;
  s.ground_normal = h.normal;
  s.ground_solid = h.solid;
  s.since_ground = 0;
  if (snap) s.pos = h.end;
}

// The water at the feet: the swimmer's level and the contents of the body there.
void read_water(RowBody& r, const ClassicRows& rows, const RowWorld& world) {
  r.waterlevel = water::immersion(world.water, rows.swim, r.body.pos, 0);
  r.watertype = kContentsEmpty;
  if (r.waterlevel == 0) return;
  const double s = rows.swim.scale;
  const water::Sample w = water::sample(world.water, r.body.pos.x / s,
                                        r.body.pos.y / s + rows.swim.feet, r.body.pos.z / s, 0);
  if (w.body >= 0) r.watertype = world.kinds[static_cast<size_t>(w.body)];
}

// A lip within reach: solid low ahead of the feet, open over it (the swimmer's `Ledge`).
bool ledge_ahead(const void* ctx, const core::Vec3& p, double heading, double /*reach*/) {
  const HullWorld& w = *static_cast<const HullWorld*>(ctx);
  const move::Vec3 ahead{core::libm::sin(heading) * kLedgeAhead, 0,
                         core::libm::cos(heading) * kLedgeAhead};
  const move::Vec3 low = move::add(p, {ahead.x, kLedgeLow, ahead.z});
  if (contents_at(w, low) != kContentsSolid) return false;
  return contents_at(w, move::add(p, {ahead.x, kLedgeHigh, ahead.z})) == kContentsEmpty;
}

struct Converted {
  move::MoveCmd cmd;
  water::SwimInput swim;
};

// The classic command's axes as the engine's stick: the wish vector over the larger of its own
// length and the row's max speed, so the stick's length times the max speed is the classic's
// wish speed and its direction the classic's.
Converted convert(const MoveCmd& c, double max_speed, bool three_d) {
  Converted out;
  move::MoveCmd& m = out.cmd;
  m.yaw = -c.angles.y * kDegrees;
  m.pitch = -c.angles.x * kDegrees;
  m.jump = (c.buttons & kButtonJump) != 0;
  const double f = c.forward, s = c.side, u = c.up;
  const double sy = core::libm::sin(m.yaw), cy = core::libm::cos(m.yaw);
  const double sp = core::libm::sin(m.pitch), cp = core::libm::cos(m.pitch);
  move::Vec3 w = move::add(move::scale({sy, 0, cy}, f), move::scale({cy, 0, -sy}, s));
  if (three_d)
    w = move::add(move::add(move::scale({sy * cp, sp, cy * cp}, f), move::scale({cy, 0, -sy}, s)),
                  {0, u, 0});
  const double len = move::length(w);
  const double k = len > max_speed ? len : max_speed;
  m.forward = f / k;
  m.side = s / k;
  m.up = u / k;
  out.swim = {w.x / k, w.y / k, w.z / k, m.jump};
  return out;
}

void swim_command(RowBody& r, const MoveCmd& cmd, const ClassicRows& rows, const RowWorld& world,
                  double dt) {
  move::MoveState& s = r.body;
  const Converted c = convert(cmd, rows.walk.max_speed, true);
  water::SwimState& w = r.swim;
  w.p = s.pos;
  w.v = s.vel;
  w.heading = c.cmd.yaw;
  // The water jump needs no button, only a lip ahead and not falling fast.
  water::SwimInput in = c.swim;
  in.jump = s.vel.y >= -kGroundSpeedLimit;
  if (c.cmd.jump) {  // swimming up, not jumping
    s.grounded = false;
    w.v.y = swim_up(r.watertype);
  }
  const water::Ledge ledge{ledge_ahead, &world.hulls};
  water::step_swimmer(world.water, rows.swim, in, w, s.t, dt, ledge);
  s.vel = w.v;  // the swimmer's velocity; the mover carries the body through the hulls
  const move::Vec3 kept = s.vel;
  move::carry_along(rows.walk, world.mover, s, dt, true);
  if (w.jumping > 0) {
    s.vel = kept;  // a water jump rides over the lip unslowed
    r.body.jump_held = true;
  } else {
    r.body.jump_held = c.cmd.jump;
  }
  probe_ground(r, rows.walk, world.mover, false);
}

// The water jump's flight: no wish and no friction; in the water (level 2 or more) no gravity and
// the step up, out of it gravity and the plain move; the velocity kept through the move either
// way. It ends falling or standing.
void water_jump(RowBody& r, const ClassicRows& rows, const RowWorld& world, double dt) {
  move::MoveState& s = r.body;
  r.swim.jumping -= dt;
  const bool in_water = r.waterlevel >= 2;
  if (!in_water) s.vel.y -= rows.swim.gravity * dt;
  const move::Vec3 kept = s.vel;
  move::carry_along(rows.walk, world.mover, s, dt, in_water);
  s.vel = kept;
  probe_ground(r, rows.walk, world.mover, true);
  if (s.grounded || r.swim.jumping <= 0) r.swim.jumping = 0;
}

void one_command(RowBody& r, const MoveCmd& cmd, const ClassicRows& rows, const RowWorld& world) {
  const double dt = cmd.msec * 0.001;
  move::MoveState& s = r.body;
  switch (r.type) {
    case MoveType::none:
    case MoveType::lock:
      break;
    case MoveType::spectator:
    case MoveType::old_spectator: {
      s.mode = move::Mode::noclip;
      move::step_move(rows.spectator, world.mover, s,
                      convert(cmd, rows.spectator.max_speed, true).cmd, dt);
      s.grounded = false;
      break;
    }
    case MoveType::fly:
      s.mode = move::Mode::fly;
      move::step_move(rows.fly, world.mover, s, convert(cmd, rows.fly.max_speed, true).cmd, dt);
      probe_ground(r, rows.fly, world.mover, false);
      break;
    case MoveType::dead: {  // friction and collision only
      MoveCmd still = cmd;
      still.forward = still.side = still.up = 0;
      still.buttons = 0;
      s.mode = move::Mode::walk;
      move::step_move(rows.walk, world.mover, s, convert(still, rows.walk.max_speed, false).cmd,
                      dt);
      break;
    }
    case MoveType::normal:
      s.mode = move::Mode::walk;
      read_water(r, rows, world);
      if (r.swim.jumping > 0 && s.vel.y < 0) r.swim.jumping = 0;
      if (r.swim.jumping > 0)
        water_jump(r, rows, world, dt);
      else if (r.waterlevel >= 2)
        swim_command(r, cmd, rows, world, dt);
      else
        move::step_move(rows.walk, world.mover, s, convert(cmd, rows.walk.max_speed, false).cmd,
                        dt);
      break;
  }
  if (r.type != MoveType::spectator && r.type != MoveType::old_spectator)
    read_water(r, rows, world);  // a spectator is never categorised
}

}  // namespace

ClassicRows classic_rows(const MoveSettings& set, float maxspeed, float entgravity) {
  ClassicRows r;
  move::MoveSpec& w = r.walk;
  w.unit = kUnitsPerMetre;
  w.radius = 16;
  w.height = w.crouch_height = 56;
  w.eye_height = w.crouch_eye = 46;
  w.max_speed = maxspeed;
  w.back_scale = w.side_scale = w.walk_scale = w.sprint_scale = w.crouch_scale = 1;
  w.accelerate = set.accelerate;
  w.friction = set.friction;
  w.stop_speed = set.stopspeed;
  w.edge_friction = kEdgeFriction;
  w.edge_probe = 0;
  w.walk_normal = 0.7;
  w.step_height = kStepHeight;
  w.ground_snap = 0;
  w.contact = 1;
  w.unground_speed = kGroundSpeedLimit;
  w.land_clip_speed = kLandingClip;
  w.friction_whole = true;
  w.clip_from_start = true;
  w.gravity = static_cast<double>(set.gravity) * entgravity;
  w.gravity_lead = 1;
  w.air_accelerate = set.accelerate;
  w.air_speed_cap = kAirWishCap;
  w.air_max_speed = set.bunnyspeedcap > 0 ? static_cast<double>(maxspeed) * set.bunnyspeedcap : 0;
  w.jump_speed = kJumpSpeed;
  w.jump_additive = true;
  w.crouch_jump = false;
  w.ladder_probe = 0;

  r.fly = w;
  r.fly.friction = kFlyFriction;
  r.fly.stop_speed = 0;
  r.spectator = w;
  r.spectator.friction = set.friction * kSpectatorFriction;
  r.spectator.max_speed = set.spectatormaxspeed;

  water::SwimSpec& s = r.swim;
  s.regime = water::Regime::kinematic;
  s.scale = kUnitsPerMetre;
  s.feet = 1 / kUnitsPerMetre;
  s.waist = 28 / kUnitsPerMetre;  // the box's middle
  s.eyes = 46 / kUnitsPerMetre;
  s.run_speed = maxspeed;
  s.wish_scale = kSwimShare;
  s.accelerate = set.wateraccelerate;
  s.friction = set.waterfriction;
  s.idle_sink = kIdleSink * kSwimShare;
  s.along_wish = true;
  s.jump_speed = kWaterJumpUp;
  s.jump_push = kWaterJumpOut;
  s.jump_time = kWaterJumpTime;
  s.gravity = w.gravity;
  return r;
}

void build_row_world(RowWorld& world) {
  world.mover = mover_world(world.hulls);
  world.kinds.clear();
  if (world.hulls.count > 0)
    add_liquids(world.hulls.models[0], world.hulls.origins ? world.hulls.origins[0] : Vec3f{},
                kUnitsPerMetre, world.water, world.kinds);
}

RowBody row_body(const MoveState& c, const ClassicRows& rows, const RowWorld& world) {
  RowBody r;
  r.type = c.type;
  r.maxspeed = c.maxspeed;
  move::MoveState& s = r.body;
  s.pos = move::sub(to_engine(c.origin), {0, kOriginOverFeet, 0});
  s.vel = to_engine(c.velocity);
  s.jump_held = c.jump_held;
  s.since_ground = move::kSinceNever;
  r.swim.jumping = c.waterjump_time;
  read_water(r, rows, world);
  if (c.type == MoveType::spectator || c.type == MoveType::old_spectator) {
    r.waterlevel = c.waterlevel;  // never categorised: the state's own labels stand
    r.watertype = c.watertype;
  }
  // Snapped down only where the classic's categorising snaps: walking, out of the water.
  const bool walking = c.type == MoveType::normal || c.type == MoveType::dead;
  if (c.type != MoveType::spectator && c.type != MoveType::old_spectator)
    probe_ground(r, rows.walk, world.mover, walking && r.waterlevel < 2);
  if (c.ground != -1 && !s.grounded) {  // the state says so: keep it, the plane flat
    s.grounded = true;
    s.ground_solid = static_cast<uint32_t>(c.ground + 1);
    s.ground_normal = {0, 1, 0};
  }
  return r;
}

void row_state(const RowBody& r, MoveState& c) {
  c.origin = to_map(move::add(r.body.pos, {0, kOriginOverFeet, 0}));
  c.velocity = to_map(r.body.vel);
  c.type = r.type;
  c.ground = r.body.grounded ? static_cast<int>(r.body.ground_solid) - 1 : -1;
  c.ground_normal = to_map(r.body.ground_normal);
  c.jump_held = r.body.jump_held;
  c.waterjump_time = static_cast<float>(r.swim.jumping);
  c.waterlevel = r.waterlevel;
  c.watertype = r.watertype;
  c.maxspeed = r.maxspeed;
}

void row_command(RowBody& r, const MoveCmd& cmd, const ClassicRows& rows, const RowWorld& world) {
  if (cmd.msec <= 50) {
    one_command(r, cmd, rows, world);
    return;
  }
  MoveCmd half = cmd;
  half.msec = static_cast<uint8_t>(cmd.msec / 2);
  row_command(r, half, rows, world);
  half.impulse = 0;
  row_command(r, half, rows, world);
}

}  // namespace ge::shooter
