// THE RESORT'S RULE BOOK'S NUMBERS (R25–R30): every number a generator from the resorts on (v8)
// builds a whole ski area to — the massif, the lifts and their stations, the network of runs and
// lanes, the hub, the wind tunnels and the courses. Each
// group's comment says what its numbers mean.
//
// Not here yet: the real faces' numbers (docs/goals.md M4d).
// Held by tests/mountain_resort_build_test.cpp (every ski area is built off these numbers).
//
// ge:summary The resort rule book's numbers (R25-R30) as constexpr namespaces
// ge:keywords resort-rules tree-line-massif massif lift network piste road hub tunnel course
// ge:keywords ski-route orange-route wild-route route-jump r42
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <array>

#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

// One profile's shape down the fall line (as terrain.hpp's, plus a bench).
struct MassifShape {
  double shoulder;
  double shoulder_run;
  double ease;
  double runout;
};

// A value per kind of lift (gondola, chair, drag), by kind.
struct PerLift {
  double gondola;
  double chair;
  double drag;
};

// A value per piste colour, in PisteGrade order.
using PerGrade = std::array<double, 4>;

namespace resort_rules {

// The mountain a region's tree-line bands are stated for (TREE_LINE_MASSIF).
inline constexpr Band tree_line_massif_vertical{900, 1150};

namespace massif {
inline constexpr double size = 4000;
inline constexpr Band vertical{1420, 1620};
inline constexpr Band sea{10, 20};
namespace relief {
inline constexpr double flank = 1.6;
inline constexpr double hills = 2;
inline constexpr double ridges = 1.8;
inline constexpr double rollers = 1;
inline constexpr double bowls = 1.8;
inline constexpr double headwalls = 1.4;
inline constexpr double scale = 2;
}  // namespace relief
namespace peak {
inline constexpr Band across{260, 520};
inline constexpr Band spread{360, 560};
}  // namespace peak
namespace shoulder {
inline constexpr Band across{480, 820};
inline constexpr Band share{0.56, 0.72};
}  // namespace shoulder
inline constexpr double ridge_floor = 0.48;
inline constexpr double ridge_wander = 0.06;
inline constexpr double ridge_scale = 420;
inline constexpr Band sector{420, 620};
inline constexpr MassifShape steep_profile{0.55, 0.07, 1.35, 0.12};
inline constexpr MassifShape gentle_profile{0.35, 0.32, 0.55, 0.32};
namespace bench {
inline constexpr Band at{0.44, 0.56};
inline constexpr Band width{0.06, 0.09};
inline constexpr Band depth{0.6, 0.8};
inline constexpr Band toward{0.35, 0.6};
inline constexpr Band spread{520, 800};
inline constexpr double floor = 0.25;
}  // namespace bench
namespace village {
inline constexpr Band across{0, 220};
}  // namespace village
inline constexpr double ridges = 1.15;
inline constexpr double gentle_folds = 0.55;
inline constexpr double floor_relief = 0.6;
namespace headwalls {
inline constexpr Band at{0.12, 0.42};
inline constexpr double across = 420;
inline constexpr Band spread{160, 320};
}  // namespace headwalls
namespace flank {
inline constexpr double inner = 1180;
inline constexpr double outer = 1420;
}  // namespace flank
}  // namespace massif

namespace lift {
inline constexpr double below = 60;
inline constexpr double clear = 14;
inline constexpr double pad = 30;
inline constexpr double pad_cut = 0.25;
inline constexpr double pad_blend = 22;
namespace top {
inline constexpr double pad = 48;
inline constexpr double deck = 7;
inline constexpr double lean = 0.11;
namespace approach {
inline constexpr double length = 90;
inline constexpr PerLift from{4, 13, 0};
inline constexpr double ease = 3;
inline constexpr PerLift wheel{6, 3.8, 0};
inline constexpr PerLift tower{16, 11, 0};
inline constexpr PerLift hang{4.3, 2.9, 0};
inline constexpr double clear = 0.8;
inline constexpr PerLift half{9, 7, 0};
inline constexpr double blend = 10;
}  // namespace approach
namespace ramp {
inline constexpr double width = 26;
inline constexpr double blend = 12;
inline constexpr double drop = 4;
inline constexpr double fall = 0.1;
inline constexpr double far = 260;
inline constexpr double least = 6;
inline constexpr double grade = 0.2;
inline constexpr double steep = 0.48;
inline constexpr double ease = 0.3;
inline constexpr double head = 30;
}  // namespace ramp
}  // namespace top
namespace unload {
inline constexpr double at = 7;
inline constexpr double height = 1.2;
inline constexpr double reach = 7;
inline constexpr double half = 6;
inline constexpr double edge = 2;
inline constexpr double back = 3;
}  // namespace unload
// A bottom station's footprint: `back` m behind its wheel, `ahead` in front, `half` either side.
struct Footprint {
  double back, ahead, half;
};
inline constexpr Footprint footprint_gondola{26, 6, 8};
inline constexpr Footprint footprint_chair{13, 11, 11};
inline constexpr Footprint footprint_drag{6, 9, 6};
namespace chain {
inline constexpr Band bearing{0.35, 1.5};
inline constexpr Band reach{20, 60};
inline constexpr std::array<std::array<double, 2>, 10> aims{{{1.35, 50},
                                                             {1.25, 46},
                                                             {1.45, 54},
                                                             {1.3, 56},
                                                             {1.4, 44},
                                                             {1.2, 52},
                                                             {1.45, 40},
                                                             {1.15, 48},
                                                             {1.35, 58},
                                                             {1.1, 42}}};
inline constexpr double fall = 0.06;
inline constexpr double grade = 0.08;
inline constexpr double rise = 0.3;
inline constexpr double half = 5;
inline constexpr double blend = 10;
inline constexpr double climb = 0.15;
inline constexpr double fill = 6;
inline constexpr double dig = 18;
inline constexpr double bank = 1.5;
inline constexpr double share = 36;
inline constexpr double lip = 12;
}  // namespace chain
namespace nursery {
inline constexpr Band at{0.74, 0.8};
inline constexpr Band across{380, 560};
}  // namespace nursery
namespace drag {
inline constexpr Band length{300, 1200};
inline constexpr double pitch = 0.42;
inline constexpr double pitch_window = 20;
inline constexpr double square = 0.6;
inline constexpr double room = 8;
inline constexpr double beside = 30;
inline constexpr double pad_grade = 0.12;
inline constexpr double let_go = 6;
inline constexpr double rim = 12;
}  // namespace drag
namespace outer {
inline constexpr Band across{380, 520};
inline constexpr Band at{0.18, 0.3};
}  // namespace outer
}  // namespace lift

namespace network {
inline constexpr double gap = 24;
inline constexpr double shared = 200;
inline constexpr double finish = 260;
inline constexpr double traverse = 0.7;
inline constexpr double sibling = 300;
inline constexpr double centre = 0.9;
inline constexpr double sweep_reach = 2400;
inline constexpr double longest = 6000;
inline constexpr double shortest = 300;
inline constexpr double junction = 120;
inline constexpr int tries = 6;
inline constexpr int runs_min = 6;
}  // namespace network

namespace piste {
inline constexpr std::array<Band, 4> width{{{40, 70}, {35, 65}, {30, 60}, {25, 50}}};
inline constexpr double open = 0.3;
inline constexpr double most = 90;
namespace neck {
inline constexpr Band every{450, 900};
inline constexpr Band length{140, 260};
inline constexpr double ease = 80;
inline constexpr PerGrade width{24, 20, 18, 14};
}  // namespace neck
}  // namespace piste

namespace road {
inline constexpr Band width{6, 10};
inline constexpr double grade = 0.12;
inline constexpr double steer = 0.85;
inline constexpr double swing = 1.45;
inline constexpr Band aim{0.06, 0.11};
inline constexpr Band reach{250, 1700};
inline constexpr double shortest = 200;
inline constexpr int tries = 2;
}  // namespace road

namespace access {
inline constexpr double skate = 70;
inline constexpr double runout = 800;
}  // namespace access

namespace hub {
inline constexpr double reach = 140;
inline constexpr double below = 150;
inline constexpr double inside = 15;
inline constexpr double rise = 4;
inline constexpr double margin = 40;
inline constexpr Band depth{100, 240};
inline constexpr double wander = 20;
inline constexpr double ease = 200;
inline constexpr double step = 20;
inline constexpr double fringe = 40;
inline constexpr double fade = 12;
}  // namespace hub

namespace tunnel {
inline constexpr double step = 4;
inline constexpr double under = 55;
inline constexpr double gap = 28;
inline constexpr double width = 9;
inline constexpr double speed = 28;
inline constexpr double length = 300;
inline constexpr double grade = 0.06;
inline constexpr double window = 20;
inline constexpr double reach = 160;
}  // namespace tunnel

namespace course {
inline constexpr Band length{800, 4800};
inline constexpr double merge = 60;
namespace gates {
inline constexpr Band spacing{70, 115};
inline constexpr double spacing_target = 90;
inline constexpr PerGrade width{22, 18, 15, 13};
inline constexpr double margin = 4;
inline constexpr Band share{0.55, 0.9};
inline constexpr double radius = 45;
}  // namespace gates
}  // namespace course

// R42 — THE SKI ROUTES (ski_routes.hpp): the ORANGE runs past black, found on the mountain as it
// lies. How many a map marks at most (each off a top of its own), `count`; the squares the face
// is searched over, `cell` m on a side; how far from a top station's middle a route leaves its
// pad, `rim` m, at `bearings` bearings round it; the most its steepest colour window may fall,
// `steepest` m per m (48°: an expert couloir, never a face only a rope skis); the pitch past
// which the ground is a CLIFF, `cliff` m per m (52°), read over `pitch_window` m along the line
// and over `baseline` m down the fall line; the least clear ground between the line and a
// trunk's bark, `open` m; how near a lift's line it may come, `lift` m; how near another run's
// corridor it may run, `clear` m, and for how far, `beside` m, before it ends on one; how far
// inside the map's edge it keeps, `edge` m; its length and least vertical; how far apart two
// routes keep, `apart` m, past their first `lead` m; the line's smoothing passes and its
// stations' spacing; the marked corridor's width and its stakes' spacing down both edges.
namespace route {
inline constexpr Band count{0, 2};
inline constexpr double cell = 10;
inline constexpr double rim = 30;
inline constexpr int bearings = 24;
inline constexpr double steepest = 1.1;
inline constexpr double cliff = 1.3;
inline constexpr double pitch_window = 20;
inline constexpr double baseline = 10;
inline constexpr double open = 1.5;
inline constexpr double lift = 16;
inline constexpr double clear = 14;
inline constexpr double beside = 60;
inline constexpr double edge = 60;
inline constexpr Band length{400, 3000};
inline constexpr double vertical = 250;
inline constexpr double lead = 120;
inline constexpr double apart = 60;
inline constexpr int smooth = 4;
inline constexpr double station = 4;
inline constexpr double width = 14;
inline constexpr double every = 15;

// THE WILD LINE (v10): what the search is drawn to and what is laid down the line it finds. A
// step's cost as a share of its length through the WOODS (a square with a trunk within `near` m
// of its middle), each metre a trunk must be felled for as `fell` metres more; along an EDGE
// (beside ground past the cliff's pitch, or on a cliff's shelf within `shelf` m of its edge),
// `edge`; the extra share a step straight down the fall line costs, `fall`; a DROP (a step two
// squares long over ground past the cliff's pitch, falling no more than `drop` m) at `huck` of
// its length, and the most a route carries, `drops`; the corridor's width through the woods;
// the line's smoothing passes, fewer so a turn stays tight.
namespace wild {
inline constexpr double woods = 0.5;
inline constexpr double near = 9;
inline constexpr double fell = 3;
inline constexpr double edge = 0.45;
inline constexpr double shelf = 18;
inline constexpr double fall = 0.7;
inline constexpr double drop = 16;
inline constexpr double huck = 0.7;
inline constexpr int drops = 3;
inline constexpr double width = 8;
inline constexpr int smooth = 2;
// THE JUMPS stood on the line's rolls: how many, the lip over the ground, THE GAP dug past the
// lip (its length down the line, longer the higher the lip, and its depth), the ramp, the
// landing and the width across, m; the least the ground steepens past the lip, m per m; the
// least arc between two, and how far from the line's ends and its drops they keep, m.
namespace jump {
inline constexpr Band count{1, 3};
inline constexpr Band height{3, 4.5};
inline constexpr Band gap{5, 9};
inline constexpr double dig = 2.5;
inline constexpr double ramp = 14;
inline constexpr double landing = 24;
inline constexpr double width = 7;
inline constexpr double roll = 0.04;
inline constexpr double spacing = 110;
inline constexpr double ends = 90;
inline constexpr double clear = 40;
}  // namespace jump
}  // namespace wild
}  // namespace route

}  // namespace resort_rules

}  // namespace ge::mountain::mapgen
