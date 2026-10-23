// The scripted subjects' scripts, their grounds and trunks, and the fixed-step body.
//
// ge:summary The scripted subjects' scripts, grounds, trunks and fixed-step body
// ge:keywords camera scenario script body step ground trunks
// ge:skill camera labs
// ge:test tests/camera_scenarios_test.cpp
#include "camera/scenarios.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::camera {

namespace libm = core::libm;

namespace {

constexpr double kStep = 1.0 / 960.0;
constexpr double kGravity = 9.81;
constexpr double kFace = 0.4663;       // tan 25 deg
constexpr double kFacePitch = 0.4363;  // 25 deg

constexpr Scenario kScenarios[] = {
    {"walk", "walking on flat ground, 1.5 to 4 m/s, looking round", 6, Ground::flat, false},
    {"strafe", "running 6 m/s with a 4 m/s strafe right, left, right", 6, Ground::flat, false},
    {"jump", "running 6 m/s with two jumps", 6, Ground::flat, false},
    {"stairs", "walking up 12 steps of 0.25 m", 6, Ground::stairs, false},
    {"crouch", "walking, crouched from 1.5 s to 3.5 s", 5, Ground::flat, false},
    {"fire", "standing and turning: shots, a hit from the right, one from the front, a blast", 6,
     Ground::flat, false},
    {"cliff", "running off a 25 m drop at 9 m/s", 7, Ground::cliff, false},
    {"carve", "S-turns down a 25 degree face to 22 m/s, banked", 8, Ground::slope, false},
    {"skid", "down the face at 18 m/s, swung across and braked", 5, Ground::slope, false},
    {"trees", "weaving at 10 m/s between trunks, one on the line", 6, Ground::flat, true},
    {"teleport", "running 8 m/s, put 40 m aside at 3 s", 6, Ground::flat, false},
};

// The trunks of `trees`: either side of the line every 6 m, and one on it at z = 42.
constexpr int kTrunks = 15;
Obstacle trunk(int i) {
  if (i == kTrunks - 1) return Obstacle{0.4, 42, 0.35, 0, 12, 1000};
  const int pair = i / 2;  // a pair every 6 m
  const double z = 8.0 + 6.0 * pair;
  return Obstacle{i % 2 ? 3.2 : -3.2, z, 0.3, 0, 12, static_cast<uint32_t>(i + 1)};
}

double ground_fn(const void* ctx, double x, double z) {
  return scenario_ground(*static_cast<const Scenario*>(ctx), x, z);
}

int trunks_fn(const void* ctx, double x, double z, double reach, Obstacle* out, int most) {
  const Scenario& sc = *static_cast<const Scenario*>(ctx);
  if (!sc.trees) return 0;
  int n = 0;
  for (int i = 0; i < kTrunks && n < most; ++i) {
    const Obstacle o = trunk(i);
    if (core::hypot(o.x - x, o.z - z) - o.radius <= reach) out[n++] = o;
  }
  return n;
}

// The share of a segment open before it goes under the ground. The trunks are not in it: a boom
// pulled in for every trunk flicking past its line jolts at the subject, so they are answered by
// the push-off (`trunks_fn`) instead (world.hpp).
double clear_fn(const void* ctx, const Vec3& a, const Vec3& b) {
  const Scenario& sc = *static_cast<const Scenario*>(ctx);
  constexpr int kSamples = 48;
  for (int k = 1; k <= kSamples; ++k) {
    const double u = static_cast<double>(k) / kSamples;
    const double x = core::lerp(a.x, b.x, u), y = core::lerp(a.y, b.y, u),
                 z = core::lerp(a.z, b.z, u);
    if (y < scenario_ground(sc, x, z)) return static_cast<double>(k - 1) / kSamples;
  }
  return 1.0;
}

struct Controls {
  double speed = 0;    // m/s along the heading
  double side = 0;     // m/s to the right
  double heading = 0;  // rad
  double look = 0;     // rad the look is turned off the heading
  double roll = 0;     // rad, the body's bank
  double crouch = 0;
  bool jump = false;  // take off now (if grounded)
};

Controls controls(const Scenario& sc, double t) {
  Controls c;
  const std::string_view id = sc.id;
  if (id == "walk") {
    c.speed = core::lerp(1.5, 4.0, core::smoothstep(0, 4, t));
    c.look = 0.6 * libm::sin(t * 0.8);
  } else if (id == "strafe") {
    c.speed = 6;
    c.side = t < 1 ? 0 : t < 2.5 ? 4 : t < 4 ? -4 : 4;
  } else if (id == "jump") {
    c.speed = 6;
    c.jump = (t >= 1.5 && t < 1.5 + kStep) || (t >= 4.0 && t < 4.0 + kStep);
  } else if (id == "stairs") {
    c.speed = 3;
  } else if (id == "crouch") {
    c.speed = 3;
    c.crouch = t >= 1.5 && t < 3.5 ? 1 : 0;
  } else if (id == "fire") {
    c.look = 0.8 * libm::sin(t * 0.6);
  } else if (id == "cliff") {
    c.speed = 9;
  } else if (id == "carve") {
    c.speed = core::lerp(8.0, 22.0, core::smoothstep(0, 6, t));
    c.heading = 0.6 * libm::sin(core::kTau * t / 3.0);
    c.roll = 0.5 * libm::cos(core::kTau * t / 3.0);
  } else if (id == "skid") {
    c.speed = t < 2 ? 18 : core::num::max(0.0, 18 - 9 * (t - 2));
    c.heading = t < 2 ? 0 : 1.22 * core::smoothstep(2, 2.5, t);
    c.roll = t < 2 ? 0 : -0.35 * core::smoothstep(2, 2.5, t);
  } else if (id == "trees") {
    c.speed = 10;
    c.heading = 0.25 * libm::sin(core::kTau * t / 2.5);
  } else if (id == "teleport") {
    c.speed = 8;
  }
  return c;
}

void push_event(ScriptRun& r, Moment kind, double size, Vec3 dir) {
  if (r.pending < kMaxEvents) r.events[r.pending++] = Event{kind, size, dir};
}

// One fixed step of the body.
void step_body(ScriptRun& r) {
  const Scenario& sc = *r.sc;
  ScriptRun::Body b = r.cur;
  const double t0 = r.t;
  const Controls c = controls(sc, t0);
  // The scheduled moments of `fire` and `teleport`.
  auto at = [t0](double when) { return t0 < when && t0 + kStep >= when; };
  if (sc.id == "fire") {
    if (at(1.0) || at(1.2) || at(1.4)) push_event(r, Moment::shot, 1, Vec3{0, 0, 0});
    if (at(2.5)) push_event(r, Moment::hit, 1, Vec3{1, 0, 0});
    if (at(3.5)) push_event(r, Moment::hit, 1, Vec3{0, 0, -1});
    if (at(4.5)) push_event(r, Moment::explosion, 0.8, Vec3{0, 0, 0});
  }
  const bool teleport = sc.id == "teleport" && at(3.0);
  // In a skid the body swings across while its way holds: the velocity keeps its direction.
  b.heading = c.heading;
  b.look = c.look;
  b.roll = c.roll;
  b.crouch = c.crouch;
  const double turn = sc.id == "skid" ? libm::atan2(b.vel.x, b.vel.z) : b.heading;
  const double sw = libm::sin(turn), cw = libm::cos(turn);
  if (b.grounded) {
    b.speed = core::approach(b.speed, c.speed, 9.0 * kStep);
    b.side = core::approach(b.side, c.side, 20.0 * kStep);
    b.vel.x = sw * b.speed + cw * b.side;
    b.vel.z = cw * b.speed - sw * b.side;
  }
  b.pos.x += b.vel.x * kStep;
  b.pos.z += b.vel.z * kStep;
  const double g = scenario_ground(sc, b.pos.x, b.pos.z);
  if (b.grounded) {
    if (c.jump) {
      b.grounded = false;
      b.vel.y = 5.0;
      b.pos.y += b.vel.y * kStep;
    } else if (g < b.pos.y - 0.35) {
      b.grounded = false;  // over an edge: falls with the speed it had along the ground
    } else {
      b.vel.y = (g - b.pos.y) / kStep;
      // A step up is taken at once (the camera smooths it); the ground's own slope as a speed.
      if (g - b.pos.y > 0.05) b.vel.y = 0;
      b.pos.y = g;
    }
  } else {
    b.vel.y -= kGravity * kStep;
    b.pos.y += b.vel.y * kStep;
    if (b.pos.y <= g) {
      push_event(r, Moment::landing, -b.vel.y, Vec3{0, -1, 0});
      b.pos.y = g;
      b.vel.y = 0;
      b.grounded = true;
    }
  }
  b.pitch = sc.ground == Ground::slope && b.grounded ? -kFacePitch * libm::cos(turn) : 0;
  if (teleport) {
    b.pos.x += 40;
    b.pos.y = scenario_ground(sc, b.pos.x, b.pos.z);
    push_event(r, Moment::teleport, 0, Vec3{0, 0, 0});
  }
  r.prev = teleport ? b : r.cur;
  r.cur = b;
  r.t = t0 + kStep;
}

}  // namespace

std::span<const Scenario> camera_scenarios() { return kScenarios; }

const Scenario* find_camera_scenario(std::string_view id) {
  for (const Scenario& s : kScenarios) {
    if (s.id == id) return &s;
  }
  return nullptr;
}

double scenario_ground(const Scenario& sc, double x, double z) {
  switch (sc.ground) {
    case Ground::flat:
      return 0;
    case Ground::stairs: {
      if (z < 5) return 0;
      const double step = std::floor((z - 5) / 0.35) + 1;
      return 0.25 * (step < 12 ? step : 12);
    }
    case Ground::cliff:
      return z < 20 ? 0 : -25;
    case Ground::slope:
      return -kFace * z + 1.5 * libm::sin(x / 9.0) + 0.6 * libm::sin(z / 7.0);
  }
  return 0;
}

World scenario_world(const Scenario& sc) {
  World w;
  w.ctx = &sc;
  w.ground = ground_fn;
  w.clear = clear_fn;
  w.obstacles = trunks_fn;
  return w;
}

ScriptRun start_script(const Scenario& sc) {
  ScriptRun r;
  r.sc = &sc;
  r.cur.pos.y = scenario_ground(sc, 0, 0);
  r.cur.speed = controls(sc, 0).speed;
  r.cur.vel.z = r.cur.speed;
  r.prev = r.cur;
  return r;
}

Subject advance_to(ScriptRun& r, double t_s) {
  while (r.t < t_s) step_body(r);
  const double u = r.t > t_s ? 1.0 - (r.t - t_s) / kStep : 1.0;
  const ScriptRun::Body& a = r.prev;
  const ScriptRun::Body& b = r.cur;
  Subject s;
  s.pos = Vec3{core::lerp(a.pos.x, b.pos.x, u), core::lerp(a.pos.y, b.pos.y, u),
               core::lerp(a.pos.z, b.pos.z, u)};
  s.vel = b.vel;
  s.heading = b.heading;
  s.q = core::from_euler(b.heading, b.pitch, b.roll);
  s.look_yaw = b.heading + b.look;
  s.look_pitch = 0;
  s.grounded = b.grounded;
  s.crouch = b.crouch;
  s.surface = r.sc->ground == Ground::slope ? 0.8 : 0.3;
  for (int i = 0; i < r.pending; ++i) s.events[s.event_count++] = r.events[i];
  r.pending = 0;
  r.read_to = t_s;
  return s;
}

}  // namespace ge::camera
