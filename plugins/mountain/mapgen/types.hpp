// THE SHAPE OF A GENERATED MAP: the contract between the generator and everything that skis,
// draws or measures one: the single piste's half (`Level`, `TrackPoint`, `Checkpoint`,
// `Kicker`, `Cliff`, `Drift`, `Weather`, `Mountain`, `GenerateOptions`, `TrackHit`) and the ski
// area a course's map is raced down (`ResortMap`: its runs, lifts, courses, village, hub and wind
// tunnels), a ski jumping hill built over a map (`SkiJumpHill`) and a venue's analytic surface
// (`VenueSurface`). The other venues and the water arrive with the milestones that build them
// (docs/goals.md).
//
// A field `generate_level` always sets is a plain member; one it sets only sometimes is a
// `std::optional`. A tree with no kind of its own is a `spruce`, and one in no clump has
// `clump` -1.
//
// ge:summary The Level and its parts: what a generated map publishes
// ge:keywords types
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/rules.hpp"
#include "save/io.hpp"

namespace ge::mountain::mapgen {

// R21 — the kinds of snow country, in the order a card offers them (REGION_IDS).
enum class RegionId : uint8_t { alpine, fell, continental, maritime };

// What a tree is (R14, R21), drawn only: TREE_KINDS' order, which the golden files index by.
enum class TreeKind : uint8_t {
  spruce,
  fir,
  pine,
  larch,
  blackspruce,
  stonepine,
  whitepine,
  lodgepole,
  hemlock,
  juniper,
  dwarfpine,
  snag,
  birch,
  aspen,
  rowan,
  alder,
  willow,
  beech,
  maple,
  ash,
};

// R23 — the four piste colours, gentlest first (PISTE_GRADES).
enum class PisteGrade : uint8_t { green, blue, red, black };

// R11 — a gate's panels: red and blue alternate down the piste, the start gate red.
enum class GateColour : uint8_t { red, blue };

struct Vec3 {
  double x;
  double y;
  double z;
};

// A point in plan: a station, a target, a corner.
struct PlanPoint {
  double x;
  double z;
};

// R26 — the kinds of lift, by name.
enum class LiftKind : uint8_t { gondola, chair, drag };
inline const char* lift_kind_name(LiftKind k) {
  return k == LiftKind::gondola ? "gondola" : k == LiftKind::chair ? "chair" : "drag";
}

// R27 — a groomed piste, or a transport lane.
enum class RunKind : uint8_t { piste, road };
inline const char* run_kind_name(RunKind k) { return k == RunKind::piste ? "piste" : "road"; }

// A snow-loaded tree. The trunk is what the skier meets; the crown is drawn.
struct TreeDef {
  double x;
  double z;
  double y;       // ground height at the trunk, m
  double height;  // m
  double radius;  // trunk collision radius at breast height, m (its age's girth, R14)
  double age;     // years
  double crown;   // crown radius at its widest, m
  TreeKind kind;  // spruce unless the forest gave it a kind
  int32_t clump;  // the clump it grew in (R14), -1 for a tree standing alone
};

// A gate across the piste. Index 0 of `Level::checkpoints` is the START GATE, the last the
// FINISH LINE (R11). A SLALOM GATE down a ski area's course (R28) stands `offset` m off the line
// (positive right of travel) on a piste `span` m wide there, its `width` its own panels'; both are
// absent on every other gate. A POLE GATE of a slalom set over a map (R31) is two poles and no
// panel: `pole` says whether it is OPEN (across the hill: its turning pole `turn` × half its
// `width` right of its centre, -1 or +1) or CLOSED (its poles one above the other, `width` m apart
// down the hill, `heading` turned across it); absent on every other gate. A speed event's gate
// (R32, R33, R36) is four poles and two panels (`panels`), `width` m between its inner poles; one
// that turns the racer names its turning pole's side in `turn` too. A ski cross's gate (R35) is a
// pair of triangular FLAGS (`flags`): a turning gate on the inside of a berm (`pole` open, `turn`
// the side of its turning pole) or a corridor gate across the course before a feature.
enum class PoleGate : uint8_t { open, closed };

struct Checkpoint {
  double x;
  double z;
  double y;
  double heading;
  double width;
  double s;  // arc length down the piste, m
  GateColour colour;
  std::optional<double> offset = std::nullopt;
  std::optional<double> span = std::nullopt;
  std::optional<PoleGate> pole = std::nullopt;
  int turn = 0;
  bool panels = false;
  bool flags = false;
};

struct TrackPoint {
  double x;
  double z;
  double y;
  double s;  // arc length from the start line, m
  double heading;
  double width;
  template <class Io>
  friend void describe(Io& io, TrackPoint& v) {
    io(v.x);
    io(v.z);
    io(v.y);
    io(v.s);
    io(v.heading);
    io(v.width);
  }
};

struct Spawn {
  double x;
  double z;
  double heading;
};

// THE MOUNTAIN as the map publishes it (R2).
struct Mountain {
  Vec3 summit;
  Vec3 base;
  double vertical;   // base to summit, m
  double altitude;   // the valley floor over the sea, m (R21's band)
  double tree_line;  // m over the sea (R14)
  double sea;        // the sea's height in the map's frame, m
};

// A crest shaped to kick a skier into the air (R4, R9). `x, z` is the LIP.
// R20 — a park kicker's size, and the landing it is BUILT with past its lip: a level `deck`, a
// slope falling over `fall` m to a floor `dig` under the line, then the run-out back up.
enum class TrickSize : uint8_t { low, medium, high };
inline const char* trick_size_name(TrickSize s) {
  return s == TrickSize::low ? "low" : s == TrickSize::medium ? "medium" : "high";
}
struct KickerShape {
  double deck, fall, dig;
};

// A kicker. A PARK kicker (R20, `T1…` in the order skied) names its size (`trick`) and its built
// landing (`shape`); every other kicker names neither.
struct Kicker {
  std::string id;  // `K1…` on the piste in the order skied, `X1…` off it
  double x;
  double z;
  double y;        // ground height at the lip, m
  double heading;  // the way a skier crosses the lip
  double height;   // lip over the ground it was shaped on, m
  double ramp;     // foot to lip, m
  double landing;  // lip to foot, m
  double width;    // full-height width across, m
  bool on_track;
  std::optional<double> s;  // the lip's arc down the piste (on-piste kickers only), m
  std::optional<TrickSize> trick = std::nullopt;
  std::optional<KickerShape> shape = std::nullopt;
  std::optional<std::string> route = std::nullopt;  // the ski route it jumps on (R42, `O1…`)
};

// A cliff a skier drops off (R22), or a DROP across the piste (R24, `on_track`). `x, z` is the
// middle of its EDGE.
struct Cliff {
  std::string id;  // `C1…` off the piste in the order laid, `D1…` across it in the order skied
  double x;
  double z;
  double y;  // ground height at the top of the edge, m
  double heading;
  double drop;     // the face's height, m
  double face;     // the face's run, edge to foot, m
  double apron;    // the landing apron's height over the country at the foot, m
  double landing;  // the apron's run, m
  double shelf;    // the shelf behind the edge, foot to edge, m
  double width;    // full-height length of the edge, m
  bool on_track;
  std::optional<double> s;  // a drop's arc down the piste, m
};

// R29 — THE HUB: the groomed band across the valley floor every finish runs out into and every
// valley lift leaves from, read as columns every `step` m from `x0`, each with its upper edge
// (`top`, toward the mountain) and its lower edge (`bottom`), z on the map.
struct Hub {
  double x0;
  double step;
  std::vector<double> top;
  std::vector<double> bottom;
};

// R28 — A COURSE down a ski area: named by the run it starts on, the colour its steepest stretch
// measures, the runs it skis in order, its length and its fall, m.
struct Course {
  std::string id;
  PisteGrade grade;
  std::vector<std::string> runs;
  double length;
  double drop;
};

// R30 — A WIND TUNNEL: a horizontal lift along the hub that blows a skier from its entrance to its
// exit, its line every few metres on the ground (`s` its arc, `heading` the way the wind blows).
struct WindTunnel {
  struct Point {
    double x, z, y, s, heading;
  };
  std::string id;
  std::vector<Point> points;
  double length;
  double width;
  double speed;
};

// How far a plan point stands off the runs a feature keeps clear of, m, read as a piste's
// distance from its line.
using DistanceTo = std::function<double(double x, double z)>;

// A stretch of a run by arc, m: a feature keeps off it.
struct ArcSpan {
  double from;
  double to;
};

// R26 — a ramp off a top: from a point of its pad's rim (`from`, at the pad's surface) down to
// where its line first meets run `run`'s snow (`to`, at that snow's height, `s` m down the run),
// `width` m across.
struct SummitRamp {
  struct Head {
    double x, y, z;
  };
  struct Foot {
    double x, y, z, s;
  };
  std::string run;
  Head from;
  Foot to;
  double width;
};

// A stretch of the piste the wind has drifted over (R17): its full-depth core, by arc, m.
struct Drift {
  double from;
  double to;
};

// The weather a map is skied under (R19).
inline constexpr double kNoSnowline = -1e9;
// No day's mean picked: the map's own air (air::climate_of).
inline constexpr double kNoMean = -1e9;
struct Weather {
  WeatherKind kind;
  double snowfall;   // 0 (nothing falling) … 1 (a blizzard)
  double fog;        // 0 … 1
  double wind;       // mean wind at 10 m, m/s
  double wind_from;  // the world heading it blows FROM
  bool evening;      // whether R19 sent the map out in the evening (R15's exception)
  // The snow line PICKED BY HAND: the world height at which half the fall is rain, rain under it
  // and snow above (air::fall_at, over engine/wind/precip.hpp's melting band). R19 deals none: a
  // dealt map's line is its day's air's (air::snowline_at, rising and falling with the day), and
  // only a sky picked by hand (`with_sky`) fixes one here.
  double snowline = kNoSnowline;
  // A THUNDERSTORM's strength (air::storm_of lays its cells off the map's seed): 0 none, 0.3 a weak
  // cell, 0.5 an ordinary one, 1 a severe one. R19 deals none; only a sky picked by hand
  // (`with_sky`) brings one.
  double thunder = 0;
  // The day's mean air at the mountain's base, C, PICKED BY HAND (`with_sky`); kNoMean: the
  // map's own (air::climate_of, off its latitude and height).
  double mean_c = kNoMean;
};

// The day (R15).
struct Sun {
  double hour;  // solar hour the run starts at
  double day_of_year;
  double latitude;
  std::optional<double> facing;  // the face's compass bearing; absent: due north (every v1 map)
};

// R27 — A RUN of the ski area as a map publishes it: its line (`track`, arc from its head), its
// colour as measured on the pressed mountain (a lane's is green), the lift whose top it leaves,
// and where it ends or starts part-way: `into` the run it merges into, the arc there, and the arc
// on THIS run from which it closes on that one; `branch` the run a branch lane leaves and the arc
// it leaves at; `to` the lift whose bottom station a link lane runs to. `drifts` by its own arc.
struct Run {
  struct Into {
    std::string run;
    double s;
    double from;
  };
  struct Branch {
    std::string run;
    double s;
  };
  std::string id;
  RunKind kind;
  PisteGrade grade;
  std::vector<TrackPoint> points;
  double length;
  std::string from;
  std::optional<Into> into;
  std::optional<Branch> branch;
  std::optional<std::string> to;
  std::vector<Drift> drifts;
};

// R26 — A LIFT as a map publishes it: its stations on the ground and the ramps off its top.
struct Lift {
  std::string id;
  LiftKind kind;
  Vec3 bottom;
  Vec3 top;
  std::vector<SummitRamp> ramps;
};

// R42 — A SKI ROUTE (ski_routes.hpp, `SR1…`): an ORANGE run marked down the mountain as it lies
// and never groomed. Its line off a top station's rim (arc from the rim, `width` the marked
// corridor), the lift it leaves the top of, the run it comes down onto and at what arc of that
// one, and its steepest colour window, m per m. A WILD line (v10) also names each DROP it goes
// over (its arc at the top and how far it falls) and the metres of it through the woods and
// along an edge.
struct SkiRoute {
  struct Into {
    std::string run;
    double s;
  };
  struct Drop {
    double s;
    double height;
  };
  std::string id;
  std::vector<TrackPoint> points;
  double length;
  std::string from;
  Into into;
  double steepest;
  bool wild = false;
  std::vector<Drop> drops;
  double woods = 0;
  double edge = 0;
};

// R25–R30 — THE SKI AREA a course's map is raced down: every run, lift and course (the courses
// the analysis let stand), which course `Level::track` is, the village, the line down the map
// from which the valley floor lies (`floor_z`), the hub and its wind tunnels, and its ski routes.
struct ResortMap {
  std::vector<Run> runs;
  std::vector<Lift> lifts;
  std::vector<Course> courses;
  std::string course;
  Vec3 village;
  double floor_z;
  Hub hub;
  std::vector<WindTunnel> tunnels;
  std::vector<SkiRoute> routes;  // R42, from v9; none before
};

// R31–R36 — the race disciplines a course is set over a built map in (discipline_rules.hpp).
enum class Discipline : uint8_t { slalom, downhill, super_g, giant_slalom, speed_ski, ski_cross };
inline const char* discipline_name(Discipline d) {
  switch (d) {
    case Discipline::slalom:
      return "slalom";
    case Discipline::downhill:
      return "downhill";
    case Discipline::super_g:
      return "super-g";
    case Discipline::giant_slalom:
      return "giant-slalom";
    case Discipline::speed_ski:
      return "speed-skiing";
    case Discipline::ski_cross:
      return "ski-cross";
  }
  return "unknown";
}

struct Level;

// A RACE COURSE set over a built map (R31–): the discipline and its run, the map it was set
// over (`base`, before any course: what the next run's course is set over), the start gate's
// arc and the finish line's on the piste, the vertical between, and THE LINE A RACER TAKES —
// points down the course, each `x` m right of the piste's line `s` m down it (a slalom's a
// cosine between each two, `slalom_line_at`; a speed event's every 4 m, `speed_line_at`). A slalom
// counts the combinations its setter laid; a speed event names its JUMPS by arc, the safety NETS
// along both edges, its SPEED TRAP (a downhill's and a super-G's) and how many gates turn. A
// SPEED-SKIING TRACK (R34) is a track of its own cut down the face (the map's `track` is the
// track): it names its TIMING ZONE, the final's start at its top, the stop on its run-out, the
// qualification's lowered start and its width. A SKI-CROSS COURSE (R35) is a line of its own
// weaving down a stretch of the piste (the map's `track` is the course): it names the STRETCH it
// was built on (arcs down the piste under it), its FEATURES in order down it, its FENCE (`nets`)
// and its width.
enum class CrossFeature : uint8_t { berm, rollers, jump, step };
inline const char* cross_feature_name(CrossFeature f) {
  switch (f) {
    case CrossFeature::berm:
      return "berm";
    case CrossFeature::rollers:
      return "rollers";
    case CrossFeature::jump:
      return "jump";
    case CrossFeature::step:
      return "step-down";
  }
  return "unknown";
}
struct RaceCourse {
  struct LinePoint {
    double s;
    double x;
  };
  struct Trap {
    double s, x, z, heading, width;
  };
  struct Nets {
    double gap, height, from, to;
  };
  Discipline discipline = Discipline::slalom;
  int run = 1;
  std::shared_ptr<const Level> base;
  double from = 0;
  double to = 0;
  double vertical = 0;
  int hairpins = 0;
  int verticals = 0;
  int delays = 0;
  std::vector<LinePoint> line;
  std::vector<double> jumps;
  std::optional<Nets> nets;
  std::optional<Trap> trap;
  int turns = 0;
  struct Zone {
    double from, to, length;  // the zone's two lines by arc, and its length along the snow, m
  };
  std::optional<Zone> zone;
  double top = 0;      // the final's start (the platform's lip), m of arc
  double qualify = 0;  // the qualification's lowered start, m of arc
  double stop = 0;     // where the run-out stops the top class, m of arc
  double width = 0;    // the track's width, m
  // A ski cross's feature: its arcs down the course; a jump's or a step-down's lip and its height
  // (a roller's crests' height), m; a berm's inside, -1 left or +1 right, and how far it turns,
  // degrees.
  struct Feature {
    CrossFeature kind;
    double from, to, lip, height;
    int side;
    double angle;
  };
  std::vector<Feature> features;
  struct Stretch {
    double from, to;  // arcs down the piste it was built over, m
  };
  std::optional<Stretch> stretch;
};

// R37 — A VENUE'S ANALYTIC SURFACE (straight_venue.hpp): a profile finer than the grid (a ski
// jump's 3 m wall under its edge), drawn every `dx` m of plan along the venue's line from (x, z)
// at `heading` (`fx`, `fz` its sine and cosine) and raised by `shift`. It answers the map's
// ground `half` m either side of the line between 0 and `end` m along it, and only `narrow_half`
// m before `narrow_until` (a raised in-run's deck, the ground far under it beside).
struct VenueSurface {
  double x = 0;
  double z = 0;
  double heading = 0;
  double fx = 0;
  double fz = 1;
  double dx = 1;
  std::vector<double> y;
  double shift = 0;
  double half = 0;
  double end = 0;
  double narrow_until = 0;
  double narrow_half = 0;

  // The surface `along` m down the line, m, read between its samples (clamped to its ends).
  double height_at(double along) const {
    const double last = double(y.size() - 1);
    double f = along / dx;
    f = f < 0 ? 0 : (f > last ? last : f);
    const size_t i = std::min(y.size() - 2, size_t(f));
    return y[i] + (y[i + 1] - y[i]) * (f - double(i)) + shift;
  }
  // Whether the surface answers at plan point (px, pz); `along` set to how far down the line.
  bool covers(double px, double pz, double& along) const {
    along = (px - x) * fx + (pz - z) * fz;
    if (!(along > 0 && along < end)) return false;
    const double across = (px - x) * fz - (pz - z) * fx;
    const double reach = along < narrow_until ? narrow_half : half;
    return across < reach && across > -reach;
  }
};

// R43 — which hill of the construction norm a ski jumping hill is built to.
enum class JumpHill : uint8_t { large, normal };
inline const char* jump_hill_name(JumpHill h) { return h == JumpHill::large ? "large" : "normal"; }

struct Level;

// R43 — A SKI JUMPING HILL as it was built over a map (ski_jump.hpp): its own `track`, every place
// down it m of plan along its line (the tower's deck at 0).
struct SkiJumpHill {
  struct Gate {
    int number;  // 1 the lowest
    double x;    // where it stands, m of plan
    double y;    // its height, m
    double e;    // its distance from the edge along the in-run, m
  };
  std::shared_ptr<const Level> base;  // the map it was built over, before any course
  JumpHill hill = JumpHill::large;
  double w = 0;   // K's distance, m
  double hs = 0;  // the hill size, m
  // The start gate's (the top gate's) and the finish line's (the fall line's) places, and the
  // vertical between them, m.
  double from = 0;
  double to = 0;
  double vertical = 0;
  // The in-run's straight's top, the curve's top, the table's start, the EDGE, P, K, L, U and the
  // out-run's end, m of plan.
  double straight = 0;
  double curve = 0;
  double table = 0;
  double edge = 0;
  double p = 0;
  double k = 0;
  double l = 0;
  double u = 0;
  double outrun = 0;
  double take_off = 0;      // the table's angle at the edge, rad, down positive
  double lip = 0;           // the edge's height over the knoll's foot under it, m
  std::vector<Gate> gates;  // the lowest first
  int design = 0;           // the design gate: the lowest reaching `speed` in calm air (an index)
  double speed = 0;         // the design speed off the edge, m/s
  // The scoring numbers: points a metre, a metre of in-run, a m/s of head and of tail wind.
  double metre = 0;
  double gate_factor = 0;
  double head_factor = 0;
  double tail_factor = 0;
  double width = 0;  // the hill's width, m
  double deck = 0;   // how far either side of the line the raised in-run's deck reaches, m
};

struct TrackIndex;  // query.hpp: the spatial hash `nearest_track_point` reads

// THE PISTE, open: from the start line (s = 0) to the finish (s = length). `index` is built
// the first time the piste is asked where a point is nearest (query.hpp), off the stations'
// x and z, which nothing moves after the walk (only `y` is written later).
struct Track {
  std::vector<TrackPoint> points;
  double length = 0;
  mutable std::shared_ptr<const TrackIndex> index;
  // Saved as its points and length; the index is built again on its first query.
  template <class Io>
  friend void describe(Io& io, Track& v) {
    io(v.points);
    io(v.length);
    if constexpr (Io::kReading) v.index.reset();
  }
};

struct Level {
  double seed = 0;
  double size = 0;               // the world is [0, size]² m
  double cell = 0;               // heightfield cell, m
  ge::core::Heightfield ground;  // terrain height, the piste's grading included
  Track track;
  std::vector<Checkpoint> checkpoints;
  Spawn spawn{};            // the start line's centreline point (R13)
  std::vector<Spawn> grid;  // one slot per skier, the player's first
  std::vector<TreeDef> trees;
  Sun sun{};
  int laps = 1;                  // always 1 (R16)
  ge::core::Heightfield packed;  // 0 powder … 1 packed piste (R10), on the ground's grid
  std::vector<Kicker> kickers;
  std::vector<Cliff> cliffs;
  Mountain mountain{};
  int attempt = 0;  // the sub-seed attempt the search accepted
  std::vector<Drift> drifts;
  Weather weather{};
  int version = 0;  // the generator version that built it (versions.hpp)
  RegionId region = RegionId::alpine;
  // R25 — the real face the map is laid off (real_face_crops.hpp's id); absent on a dealt map.
  std::optional<std::string> face;
  std::optional<PisteGrade> grade;             // the grade built to (R23); absent on v1
  std::optional<ge::core::Heightfield> crust;  // the region's wind crust (R21), if it lays one
  std::optional<ResortMap> resort;             // the ski area (R25–R30); absent on v1
  std::optional<RaceCourse> race;              // a course set over the map (R31–); else absent
  std::optional<SkiJumpHill> ski_jump;         // a ski jumping hill built over it (R43)
  std::shared_ptr<const VenueSurface> venue;   // a venue's analytic surface (R37), else none

  // The snow's height: a venue's own surface where it answers, the grid everywhere else.
  double ground_at(double x, double z) const {
    double along = 0;
    if (venue && venue->covers(x, z, along)) return venue->height_at(along);
    return ge::core::sample_field(ground, x, z);
  }
  double packed_at(double x, double z) const { return ge::core::sample_field(packed, x, z); }
  // The ground's unit normal off the bilinear patch's own gradient (the compile's normal).
  void normal_at(double x, double z, Vec3& out) const;
};

// What a caller may ask of the generator beyond the seed (the single-piste half).
struct GenerateOptions {
  std::optional<int> attempts;  // sub-seeds the search may try (default 16)
  std::optional<int> laps;      // default R16's one
  std::optional<int> version;   // versions.hpp; the current rules when absent or unknown
  std::optional<RegionId> region;
  // R25 — lay the ski area off this real face (its id), in its region; searched over
  // `massif::real::attempts` unless `attempts` is given.
  std::optional<std::string> face;
  // …leaning its lifts and runs onto the real ski area's (real_hints.hpp); false builds the face
  // as if it had none, which is what the hints are measured against.
  bool hints = true;
  std::optional<int> only;  // build this one attempt alone and throw its refusal
  // R28 — on a ski area, the course to race by its id, else one of this colour (the nearest there
  // is), else the colour the seed deals (`deal_grade`).
  std::optional<std::string> course;
  std::optional<PisteGrade> grade;
  // R20 — lay the terrain park down the piste raced (a map for a tricks run).
  bool tricks = false;
};

// The answer to "where on the piste is this point nearest?"
struct TrackHit {
  int index = 0;        // the station starting the nearest segment
  double s = 0;         // arc of the nearest centreline point, m
  double distance = 0;  // plan distance to the centreline, m
  double lateral = 0;   // signed offset, positive RIGHT of travel, m
  double x = 0;
  double z = 0;
};

}  // namespace ge::mountain::mapgen
GE_SAVE_PLAIN(ge::mountain::mapgen::TrackPoint);
