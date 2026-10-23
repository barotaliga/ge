// THE CROWD'S NUMBERS (crowd.hpp): who else is out on the ski area on a free ride, what kind of
// skier each of them is and how they come. A ski area on a good day is hundreds of people:
// families strung out down the green with the children in a wedge, a ski school's snake of small
// helmets in the instructor's track, friends who stop together in the middle of the run, the one
// who stops just below the crest where nobody above sees him, the bomber in a tuck, the beginner
// side-slipping a pitch he should not be on, and the lot down from the hut after lunch.
//
// Every one of them is a KIND (`kKinds`: a band for each of his knobs, his turn styles, the bodies
// he comes in) dealt in a GROUP (`kGroups`), so the runs fill the way they do: more families
// than bombers, more people on the greens than on the blacks (`kShare`). The numbers are a
// crowd's and rough on purpose: an amateur is stepped along his run's line (crowd.hpp), not by
// the skier's physics.
//
// ge:summary The crowd's numbers: bodies, turn styles, kinds, groups, the pace, falls, stops, lifts
// ge:keywords crowd crowd-rules amateur body turn-style kind group family school friends
// ge:keywords cruiser carver bomber beginner stopper drunk instructor kid share needs knock
// ge:skill sim
// ge:test tests/fall_line_crowd_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fall_line::sim::crowd {

// WHO IS IN THE KIT: the figure the app draws, and the size the player meets him with.
enum class Body : uint8_t { man, woman, teen, child, old_man, old_woman, freerider, retro };
inline constexpr size_t kBodies = 8;
const char* body_name(Body b);

// Each body's mass, kg, and the radius of the one plan circle he is to the player, m (the body
// with its arms and poles out); a child is half the mass and a smaller circle.
struct Size {
  double mass;
  double radius;
};
inline constexpr std::array<Size, kBodies> kSizes{{
    {85, 0.55},  // man
    {68, 0.50},  // woman
    {60, 0.50},  // teen
    {32, 0.38},  // child
    {82, 0.55},  // old man
    {66, 0.50},  // old woman
    {88, 0.58},  // freerider
    {72, 0.52},  // retro
}};

// HOW HE TURNS, the line he draws down the piste:
// - `line`     straight down it, the bomber's: no turns at all;
// - `carve`    long round arcs across half the piste;
// - `shorts`   quick short swings down the middle, a metre or two each way;
// - `zigzag`   from one edge to the other and back, sharp at each end;
// - `plough`   a slow wedge and gentle turns: the child and the beginner;
// - `traverse` the whole width at a time, a long way across for little way down;
// - `slip`     the falling leaf: skis across the hill, side-slipping a pitch past him.
enum class Style : uint8_t { line, carve, shorts, zigzag, plough, traverse, slip };
inline constexpr size_t kStyles = 7;

// A style's shape: how much of the half-width its turns sweep (`amp`: a share of the piste's
// half-width, or metres when `metres`), how far down the run one turn takes (m, a band), the most
// the skis turn off the line (rad), and whether it turns at the ends (a triangle wave) rather than
// in round arcs.
struct StyleDef {
  double amp;
  bool metres;
  double len_lo, len_hi;
  double max_yaw;
  bool sharp;
};
inline constexpr std::array<StyleDef, kStyles> kStyleDefs{{
    {0, false, 200, 200, 0.3, false},    // line
    {0.55, false, 24, 40, 0.85, false},  // carve
    {1.8, true, 7, 11, 0.75, false},     // shorts
    {0.8, false, 26, 50, 1.2, true},     // zigzag
    {0.35, false, 12, 22, 0.6, false},   // plough
    {0.85, false, 70, 130, 1.35, true},  // traverse
    {0.4, false, 14, 24, 1.45, true},    // slip
}};

// THE KINDS of skier on the mountain, each knob a band [lo, hi] of 0..1:
// - `skill`      how well he skis: his pace, how tight he turns, how often he falls, which
//                colours he is let on;
// - `aggression` how much of his pace he takes and how little room he gives;
// - `width`      how much of his style's sweep he uses;
// - `wobble`     how drunk he is: the line wanders, the body sways, he falls for nothing;
// - `stopper`    how often he stops in the middle of the run, the likelier just below a crest.
enum class Kind : uint8_t {
  cruiser,
  carver,
  bomber,
  beginner,
  zigzag,
  stopper,
  drunk,
  instructor,
  kid
};
inline constexpr size_t kKindCount = 9;
const char* kind_name(Kind k);

struct Band {
  double lo, hi;
};
struct KindDef {
  Band skill, aggression, width, wobble, stopper;
  std::array<Style, 4> styles;  // dealt evenly; a repeat weighs one twice
  std::array<Body, 6> bodies;   // the same
};

inline constexpr std::array<KindDef, kKindCount> kKinds{{
    // THE CRUISER: the bulk of any ski area, round turns at a pace he can stop from.
    {{0.35, 0.65},
     {0.2, 0.55},
     {0.5, 0.9},
     {0, 0.05},
     {0.05, 0.25},
     {Style::carve, Style::carve, Style::shorts, Style::zigzag},
     {Body::man, Body::woman, Body::old_man, Body::old_woman, Body::retro, Body::teen}},
    // THE CARVER: the good skier, long arcs on his edges, fast and clean.
    {{0.75, 0.95},
     {0.45, 0.8},
     {0.7, 1},
     {0, 0.02},
     {0, 0.08},
     {Style::carve, Style::carve, Style::shorts, Style::carve},
     {Body::man, Body::woman, Body::teen, Body::retro, Body::old_man, Body::man}},
    // THE BOMBER: straight down the fall line in a tuck, faster than his skiing.
    {{0.45, 0.7},
     {0.9, 1},
     {0, 0.2},
     {0, 0.05},
     {0, 0.03},
     {Style::line, Style::line, Style::line, Style::line},
     {Body::man, Body::teen, Body::freerider, Body::man, Body::teen, Body::freerider}},
    // THE BEGINNER: a wedge and a prayer.
    {{0.05, 0.25},
     {0, 0.2},
     {0.4, 0.8},
     {0.02, 0.1},
     {0.2, 0.5},
     {Style::plough, Style::plough, Style::traverse, Style::plough},
     {Body::man, Body::woman, Body::old_man, Body::old_woman, Body::retro, Body::teen}},
    // THE ZIGZAG: wall to wall across the piste, in nobody's line and everybody's way.
    {{0.2, 0.45},
     {0.1, 0.4},
     {0.85, 1},
     {0, 0.06},
     {0.1, 0.3},
     {Style::zigzag, Style::traverse, Style::zigzag, Style::traverse},
     {Body::man, Body::woman, Body::old_man, Body::old_woman, Body::retro, Body::teen}},
    // THE ONE WHO STOPS BELOW THE CREST: mid-piste just over the roll, unseen from above.
    {{0.2, 0.5},
     {0, 0.3},
     {0.3, 0.6},
     {0, 0.05},
     {0.7, 1},
     {Style::carve, Style::plough, Style::shorts, Style::carve},
     {Body::man, Body::woman, Body::old_man, Body::old_woman, Body::retro, Body::man}},
    // AFTER LUNCH AT THE HUT: the line wanders, the body sways and the snow comes up to meet him.
    {{0.25, 0.6},
     {0.4, 0.9},
     {0.4, 1},
     {0.55, 1},
     {0.2, 0.5},
     {Style::zigzag, Style::carve, Style::line, Style::zigzag},
     {Body::man, Body::man, Body::woman, Body::old_man, Body::retro, Body::man}},
    // THE INSTRUCTOR at the head of a ski school: slow, short, tidy turns the children follow.
    {{0.85, 1},
     {0, 0.1},
     {0.35, 0.55},
     {0, 0},
     {0.15, 0.3},
     {Style::plough, Style::shorts, Style::plough, Style::shorts},
     {Body::man, Body::woman, Body::man, Body::woman, Body::man, Body::woman}},
    // A CHILD: in a wedge behind whoever is in front, and down a dozen times a morning.
    {{0.1, 0.45},
     {0.1, 0.5},
     {0.3, 0.7},
     {0.03, 0.12},
     {0.1, 0.3},
     {Style::plough, Style::plough, Style::shorts, Style::plough},
     {Body::child, Body::child, Body::child, Body::child, Body::child, Body::child}},
}};

// HOW PEOPLE COME: the group kinds and what each is made of.
// - `solo`    one of anyone;
// - `couple`  two adults;
// - `family`  a parent and one to four behind, a child first; the parent waits for the last;
// - `friends` two to four who stop together;
// - `school`  an instructor and a snake of three to seven children in his very track;
// - `drunks`  two or three down from the hut.
enum class GroupKind : uint8_t { solo, couple, family, friends, school, drunks };
inline constexpr size_t kGroupKinds = 6;
const char* group_name(GroupKind g);

struct GroupDef {
  double weight;  // how often the group is dealt, against the others
  std::array<Kind, 8> lead;
  std::array<Kind, 6> follow;
  int32_t follow_lo, follow_hi;  // how many follow him
  bool track;                    // in his very track (a school's snake), else each in his own line
  double gap;                    // how far apart down the run they ski, m
};

inline constexpr std::array<GroupDef, kGroupKinds> kGroups{{
    {30,
     {Kind::cruiser, Kind::cruiser, Kind::carver, Kind::bomber, Kind::beginner, Kind::zigzag,
      Kind::stopper, Kind::drunk},
     {Kind::cruiser, Kind::cruiser, Kind::cruiser, Kind::cruiser, Kind::cruiser, Kind::cruiser},
     0,
     0,
     false,
     0},
    {9,
     {Kind::cruiser, Kind::cruiser, Kind::carver, Kind::beginner, Kind::zigzag, Kind::cruiser,
      Kind::cruiser, Kind::carver},
     {Kind::cruiser, Kind::cruiser, Kind::beginner, Kind::zigzag, Kind::cruiser, Kind::beginner},
     1,
     1,
     false,
     8},
    {12,
     {Kind::cruiser, Kind::cruiser, Kind::cruiser, Kind::cruiser, Kind::cruiser, Kind::cruiser,
      Kind::cruiser, Kind::cruiser},
     {Kind::kid, Kind::kid, Kind::kid, Kind::cruiser, Kind::kid, Kind::kid},
     1,
     4,
     false,
     7},
    {8,
     {Kind::cruiser, Kind::carver, Kind::bomber, Kind::cruiser, Kind::carver, Kind::bomber,
      Kind::cruiser, Kind::carver},
     {Kind::cruiser, Kind::cruiser, Kind::carver, Kind::bomber, Kind::zigzag, Kind::cruiser},
     1,
     3,
     false,
     10},
    {4,
     {Kind::instructor, Kind::instructor, Kind::instructor, Kind::instructor, Kind::instructor,
      Kind::instructor, Kind::instructor, Kind::instructor},
     {Kind::kid, Kind::kid, Kind::kid, Kind::kid, Kind::kid, Kind::kid},
     3,
     7,
     true,
     4},
    {3,
     {Kind::drunk, Kind::drunk, Kind::drunk, Kind::drunk, Kind::drunk, Kind::drunk, Kind::drunk,
      Kind::drunk},
     {Kind::drunk, Kind::drunk, Kind::drunk, Kind::drunk, Kind::drunk, Kind::drunk},
     1,
     2,
     false,
     9},
}};

// HOW CROWDED EACH COLOUR IS, per metre of run (green, blue, red, black, then a road): the greens
// full, the blacks nearly empty, a cat track a thoroughfare between them; and the skill a colour
// asks for: below it a skier is on it only as the one his friends took up there (`kLost`, the
// share of the weight he keeps).
inline constexpr std::array<double, 5> kShare{1, 0.6, 0.3, 0.12, 0.25};
inline constexpr std::array<double, 5> kNeeds{0, 0.3, 0.5, 0.72, 0};
inline constexpr double kLost = 0.05;

// How many amateurs a free ride deals when asked for a crowd, and the most it takes.
inline constexpr int32_t kCount = 400;
inline constexpr int32_t kMost = 600;
// The share of the groups dealt riding a lift part-way up (a real ride takes minutes, so on a
// mountain open all morning a good part of the crowd is on the lifts), and of the rest dealt into
// a lift's queue rather than onto the snow.
inline constexpr double kRiding = 0.3;
inline constexpr double kQueued = 0.15;
// How long a group with no lift to queue at is up one unseen between runs, s.
inline constexpr Band kUnseenLift{12, 45};

// How often an amateur reconsiders his line, s; every one at his own phase of it.
inline constexpr double kThinkS = 0.25;

// THE PACE he skis at, m/s: `kBaseSpeed` and up to `kSpanSpeed` more by his skill, as much of it
// as his aggression takes; the bomber's line runs `kLineSpeed` times that; a slip is `kSlipSpeed`;
// on a colour past him he holds to `kPastBase` + `kPastSkill` * skill.
inline constexpr double kBaseSpeed = 3.5;
inline constexpr double kSpanSpeed = 15;
inline constexpr double kLineSpeed = 1.6;
inline constexpr double kSlipSpeed = 2.2;
inline constexpr double kPastBase = 2.5;
inline constexpr double kPastSkill = 5;
// The resistance on him: the snow's, m/s²; the air's on the body, per m (a v² drag); the scrub of
// his skis turned off the way he goes, m/s² a radian.
inline constexpr double kSnowDrag = 0.45;
inline constexpr double kAirDrag = 0.0035;
inline constexpr double kScrub = 1.4;
// How hard he checks his speed, m/s²; the push his poles and his skating give him where the hill
// does not carry him, m/s², eased off over the last `kPushEase` m/s below the most it reaches,
// `kPushTop` m/s; and the speed under which he is at a crawl, m/s (a stop's clock runs there).
inline constexpr double kBrake = 3.5;
inline constexpr double kPush = 0.9;
inline constexpr double kPushEase = 1;
inline constexpr double kPushTop = 3.5;
inline constexpr double kCrawl = 2.6;
// The speed a stop is stood at, m/s.
inline constexpr double kStand = 0.3;
// How fast he swings his skis round, rad/s, at no skill and at all.
inline constexpr Band kYawRate{1.4, 3.4};

// A FALL for nothing: how many a minute at no skill on a green, and how much likelier on a colour
// past him (per unit of skill short, times four) and drunk (per unit of wobble, times three); how
// long he lies once he has slid to a stop, s; how fast he slides to it, m/s²; and how long he
// takes to get up, s.
inline constexpr double kFallRate = 0.18;
inline constexpr double kFallSteep = 3;
inline constexpr double kFallWobble = 1.4;
inline constexpr Band kLie{1, 4};
inline constexpr double kSlideDecel = 4;
inline constexpr double kRiseS = 1.4;

// A STOP: how many a minute at a stopper's 1, how long, s; and how much likelier just below a
// crest, a roll he has just come over, by how much steeper the pitch is here than `kCrestBack` m
// above (`kCrestPitch`, rise over run).
inline constexpr double kStopRate = 1.2;
inline constexpr Band kStopHold{3, 12};
inline constexpr double kCrest = 6;
inline constexpr double kCrestBack = 12;
inline constexpr double kCrestPitch = 0.08;

// THE ROOM he keeps from the skier ahead on his run: how far ahead he looks, m and s of his
// speed; how wide a berth, m; how far behind one still counts as level with him, m; and in a
// school's snake, the share of the pace ahead he drops to closer than half his look.
inline constexpr double kRoomAhead = 4;
inline constexpr double kRoomTime = 0.8;
inline constexpr double kBerth = 1.8;
inline constexpr double kLevel = 1.5;
inline constexpr double kSnakeBack = 0.6;

// A GROUP regrouping: how far behind the last may fall before the leader waits, m, how near he
// must come before they go on, and how far down a run off a lift's top the leader waits there for
// the rest still on it, m.
inline constexpr double kRegroupFar = 60;
inline constexpr double kRegroupNear = 15;
inline constexpr double kRegroupTop = 80;

// THE LIFTS RIDDEN: how far from a run's end a lift's load point is looked for, m; how near a
// run's line one must be for the run to pass it, m, and the share of the groups passing one part
// way down a run whose leader takes it; how fast he skates
// to his place in its queue or off its top onto his run, m/s; the rows of its queue, m apart, and
// the riders side by side in a row, m apart (a row is a carrier's seats); how near his place he is
// stood in it, m; and how far down a run off a lift's top he skates from its ramp's foot onto it,
// m.
inline constexpr double kLiftReach = 150;
inline constexpr double kStopReach = 60;
inline constexpr double kTakeStop = 0.4;
inline constexpr double kSkate = 3;
inline constexpr double kQueueRowM = 1.8;
inline constexpr double kQueueSideM = 0.9;
inline constexpr double kAtPlaceM = 0.6;
inline constexpr double kOntoM = 4;
// A carrier's seats by lift kind (gondola, chair, drag).
inline constexpr std::array<int32_t, 3> kSeats{8, 4, 2};

// THE PLAYER MEETING ONE: the player's own circle, m; the closing speed that knocks the amateur
// down at no skill and at all, m/s; the share of it that comes back; and the closing speed that
// writes a `bump` event, m/s.
inline constexpr double kPlayerRadius = 0.6;
inline constexpr Band kKnock{2.5, 6};
inline constexpr double kRestitution = 0.25;
inline constexpr double kBumpEvent = 1;
// The player's mass the push is shared by, kg (a skier and his kit).
inline constexpr double kPlayerMass = 85;

}  // namespace fall_line::sim::crowd
