// THE CROWD AS A RUN HOLDS IT (`GameState::crowd`): every amateur on the ski area, his group and
// the crowd's own stream. Its own header so state.hpp can carry it without the rules that move it
// (crowd.hpp).
//
// A run dealt no crowd leaves it inactive, hashes none of it and moves no digest.
//
// ge:summary The crowd as a run holds it: CrowdState, Amateur, AmateurMode, CrowdGroup
// ge:keywords crowd crowd-state amateur amateur-mode group knobs queue ride skate lift
// ge:skill sim
// ge:test tests/fall_line_crowd_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "core/prng.hpp"
#include "fall_line/sim/crowd_rules.hpp"

namespace fall_line::sim {

// What an amateur is doing: on his run skiing (`ski`), stopped on it (`stop`) or down in the
// snow (`down`: sliding to a stop, lying, getting up); at a lift's foot skating to his place in
// its queue and stood in it (`queue`); carried up on one of its carriers (`ride`); skating off
// its top to his run (`skate`); or up a lift unseen, on a map with none to queue at (`unseen`).
enum class AmateurMode : uint8_t { ski, stop, down, queue, ride, skate, unseen };
const char* amateur_mode_name(AmateurMode m);

// What makes one amateur himself, dealt once from his kind's bands (crowd_rules.hpp), each 0..1:
// `turn` is one turn's length down the run, m.
struct Knobs {
  double skill = 0;
  double aggression = 0;
  double width = 0;
  double wobble = 0;
  double stopper = 0;
  crowd::Style style = crowd::Style::carve;
  double turn = 20;
};

// ONE OF THE CROWD: kept as where he is along his run (`run`, `s`) and across it (`d`, m right of
// its line), how fast he goes and how far his skis point off the line, and, written every step for
// the player and the picture alike, where that puts him on the mountain. The pose numbers (`lean`
// … `plough`) are what the figure is drawn with; nothing in the step reads them back.
struct Amateur {
  int32_t id = 0;
  int32_t group = 0;
  int32_t rank = 0;  // his place in the group, 0 the leader
  crowd::Body body = crowd::Body::man;
  crowd::Kind kind = crowd::Kind::cruiser;
  Knobs knobs;
  AmateurMode mode = AmateurMode::ski;
  int32_t run = 0;      // by index into the free ride's runs (`GameState::runs`)
  double s = 0;         // m down the run
  double d = 0;         // m right of its line
  double speed = 0;     // m/s over the snow
  double yaw = 0;       // rad, his skis' heading off the run's line, + right
  double centre = 0;    // m, the lateral his turns sweep about
  double phase = 0;     // rad, the turn's phase, by how far down the run he is
  double cap = 0;       // m/s, the pace he means to ski at just now
  double timer = 0;     // s left in a stop, on the snow, getting up
  double think = 0;     // s to his next decision
  bool rising = false;  // down: getting up
  // At a lift: which, his ticket in its queue, the carrier that has him and his seat on it; off
  // its top, the run he skates to, the leg of the way (0 to the ramp's head, 1 down it, 2 onto the
  // run's snow) and where it ends.
  int32_t lift = -1;
  int32_t ticket = 0;
  int32_t carrier = -1;
  int32_t seat = 0;
  int32_t leg = 0;
  double tx = 0, tz = 0;
  // Where he is: his feet, the way he faces and goes, world frame.
  double x = 0, y = 0, z = 0;
  double heading = 0;
  double vx = 0, vz = 0;
  // The figure: leaned into the turn, rad (+ right); how low, 0..1; the wedge, 0..1.
  double lean = 0;
  double crouch = 0;
  double plough = 0;
};

// A GROUP: its kind, who is in it (the leader first), how it follows, the gap down the run between
// them, the run it skis next off a lift's top (-1 not chosen), how long it is still up a lift
// unseen, s, and the lift its leader took off a run part-way down it (the run and the arc: each of
// them takes it there too; -1 none).
struct CrowdGroup {
  crowd::GroupKind kind = crowd::GroupKind::solo;
  std::vector<int32_t> members;
  bool track = false;
  double gap = 0;
  int32_t next = -1;
  double unseen = 0;
  int32_t stop_lift = -1;
  int32_t stop_run = -1;
  double stop_s = 0;
};

// A lift's queue: the tickets handed out and the ones taken up.
struct CrowdQueue {
  int32_t issued = 0;
  int32_t served = 0;
};

// THE NETWORK the crowd skis, read off the map once at the deal: each run's colour (0 green … 3
// black, 4 a road), its length, the run it merges into and at what arc of that one (-1 none),
// whether a group may choose it off a lift's top, the lift at its foot (-1 none: up one unseen)
// and the lifts it passes on the way (`stops`: each lift's load point within `kStopReach` of
// its line, at the arc nearest it). Each lift's ways off its top: the run, where its ramp leaves
// the pad and where it meets the run's snow, and at what arc of the run.
struct CrowdStop {
  int32_t lift = -1;
  double s = 0;
};
struct CrowdRun {
  int32_t grade = 0;
  double length = 0;
  int32_t into = -1;
  double into_s = 0;
  bool top = false;
  int32_t foot = -1;
  std::vector<CrowdStop> stops;
};
struct CrowdRamp {
  int32_t run = -1;
  double fx = 0, fz = 0;
  double tx = 0, tz = 0;
  double s = 0;
};

struct CrowdState {
  bool active = false;  // a crowd was dealt this run: only then is any of it stepped or hashed
  ge::core::Rng rng{};  // the crowd's own stream, off the map's seed (crowd.cpp's salt)
  std::vector<CrowdRun> net;
  std::vector<std::vector<CrowdRamp>> ramps;  // one list a lift
  std::vector<Amateur> amateurs;
  std::vector<CrowdGroup> groups;
  std::vector<CrowdQueue> queues;  // one a lift
  // Scratch, sized at the deal so a step never allocates: who is on each run this step.
  std::vector<std::vector<int32_t>> on_run;
};

}  // namespace fall_line::sim
