// GROUND ZERO'S TRAINS AND FANS: the pack's changes to `func_train` and `func_rotating`, run only
// under its switch (s.opt.pack). The facts are the clean-room note's
// (porting/shooters-edge/gz-world.md § 6).
//
// - **A corner's speed**: a `path_corner` may carry `speed`, `accel` and `decel`. A train leaving
//   that corner takes the corner's speed, and its accel and decel from the corner or equal to that
//   speed. A leg whose accel or decel differ from its speed is the accelerated move, which reads
//   all three as units per 0.1 s frame.
// - **A train's team**: whatever shares the train's `team` (but another train, a door or a
//   turning mover) moves with it, by the train's own velocity each frame, so it covers the same
//   ground at the same speeds; the train is the team's master, so one part blocked holds them all.
// - **A fan's ACCEL** (`func_rotating` spawnflag 8192): it gains `accel` degrees a second (1) every
//   0.1 s up to its speed and loses `decel` (1) every 0.1 s when stopped, both at most the speed.
//   Under the pack every fan fires its targets when it starts (with ACCEL, on reaching full speed)
//   and when it stops (with ACCEL, on reaching zero).
//
// ge:summary Ground Zero's trains and fans: a corner's speeds, a train's team riding, a fan's ramp
// ge:keywords q2 ground-zero func-train path-corner corner-speed accelerated-move train-team
// ge:keywords func-rotating accel decel fan-ramp mission-pack
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_trains_test.cpp
#include <algorithm>
#include <cstdlib>
#include <string>

#include "brush/map.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_map_kinds.hpp"

namespace shooters_edge::server::q2map {

namespace {

constexpr int kAccelFan = 8192;  // func_rotating's ACCEL

bool pack(const Q2Server& s) { return s.opt.pack == Q2Pack::ground_zero; }

float number(const Q2Server& s, const Q2Entity& e, const char* name, float fallback) {
  if (e.block < 0 || size_t(e.block) >= s.map->entities.size()) return fallback;
  const std::string& v = s.map->entities[size_t(e.block)].get_any_case(name);
  return v.empty() ? fallback : std::strtof(v.c_str(), nullptr);
}

bool rides(const Q2Entity& e) {
  const Class c = class_of_name(e.classname);
  return c != kTrain && c != kDoor && c != kDoorRotating && c != kRotating;
}

void fire(Q2Server& s, Q2Entity& e) {
  map_use_targets(s, static_cast<int>(&e - s.entities.data()), e.activator);
}

}  // namespace

void gz_train_corner(const Q2Server& s, Q2Entity& train, int leaving) {
  if (!pack(s) || leaving < 0 || size_t(leaving) >= s.entities.size()) return;
  const Q2Entity& corner = s.entities[size_t(leaving)];
  const float speed = number(s, corner, "speed", 0);
  if (speed <= 0) return;
  train.speed = speed;
  train.accel = number(s, corner, "accel", speed);
  train.decel = number(s, corner, "decel", speed);
}

void gz_train_teams(Q2Server& s) {
  if (!pack(s)) return;
  for (size_t i = 0; i < s.entities.size(); i++) {
    const Q2Entity& t = s.entities[i];
    // A train with a team is its own team's master by then (q2_map.cpp's link_doors).
    if (t.gone || t.team.empty() || class_of_name(t.classname) != kTrain || t.team_master != int(i))
      continue;
    for (Q2Entity& m : s.entities)
      if (!m.gone && m.team == t.team && m.team_master < 0 && rides(m))
        m.team_master = static_cast<int>(i);
  }
}

void gz_movers_frame(Q2Server& s) {
  if (!pack(s)) return;
  for (Q2Entity& m : s.entities) {  // a train's riders take its velocity for this frame's push
    if (m.gone || m.team_master < 0 || !rides(m)) continue;
    const Q2Entity& t = s.entities[size_t(m.team_master)];
    if (class_of_name(t.classname) != kTrain) continue;
    m.velocity = t.arrive != kNoAct ? t.velocity : sh::Vec3f{};
    m.moved = true;
  }
  if (!s.tenth) return;
  for (Q2Entity& e : s.entities) {  // the fans that ramp, a step every 0.1 s
    if (e.gone || (e.spawnflags & kAccelFan) == 0 || class_of_name(e.classname) != kRotating)
      continue;
    if (e.state == kGoing) {
      e.now_speed = std::min(e.now_speed + e.accel, e.speed);
      if (e.now_speed >= e.speed) e.state = kAtSecond;
    } else if (e.state == kComing) {
      e.now_speed = std::max(e.now_speed - e.decel, 0.0f);
      if (e.now_speed <= 0) e.state = kAtFirst;
    } else {
      continue;
    }
    e.avelocity = e.axis * e.now_speed;
    if (e.state == kAtSecond || e.state == kAtFirst) fire(s, e);
  }
}

void gz_fan_spawn(const Q2Server& s, Q2Entity& e) {
  if (!pack(s) || (e.spawnflags & kAccelFan) == 0) return;
  e.accel = std::min(number(s, e, "accel", 1), e.speed);
  e.decel = std::min(number(s, e, "decel", 1), e.speed);
  e.now_speed = 0;
  e.avelocity = {};
  e.state = (e.spawnflags & 1) != 0 ? kGoing : kAtFirst;  // START_ON: it spins up
}

bool gz_fan_use(Q2Server& s, Q2Entity& e, int activator) {
  if (!pack(s)) return false;
  e.activator = activator;
  if ((e.spawnflags & kAccelFan) != 0) {  // slows from where it is, or spins up from where it is
    e.state = e.state == kGoing || e.state == kAtSecond ? kComing : kGoing;
    return true;
  }
  const bool on = e.avelocity.x == 0 && e.avelocity.y == 0 && e.avelocity.z == 0;
  e.avelocity = on ? e.axis * e.speed : sh::Vec3f{};
  fire(s, e);  // started or stopped at once: its targets fire
  return true;
}

}  // namespace shooters_edge::server::q2map
