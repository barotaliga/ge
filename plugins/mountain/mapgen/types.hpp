// THE SHAPE OF A GENERATED MAP: the contract between the generator and everything that skis,
// draws or measures one: the single piste's half (`Level`, `TrackPoint`, `Checkpoint`,
// `Kicker`, `Cliff`, `Drift`, `Weather`, `Mountain`, `GenerateOptions`, `TrackHit`). The ski
// area's fields (`resort`, the courses, the venues,
// the water) arrive with the milestone that builds them (docs/goals.md M4b–M4d).
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

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/rules.hpp"

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
// FINISH LINE (R11).
struct Checkpoint {
  double x;
  double z;
  double y;
  double heading;
  double width;
  double s;  // arc length down the piste, m
  GateColour colour;
};

struct TrackPoint {
  double x;
  double z;
  double y;
  double s;  // arc length from the start line, m
  double heading;
  double width;
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
struct Weather {
  WeatherKind kind;
  double snowfall;   // 0 (nothing falling) … 1 (a blizzard)
  double fog;        // 0 … 1
  double wind;       // mean wind at 10 m, m/s
  double wind_from;  // the world heading it blows FROM
  bool evening;      // whether R19 sent the map out in the evening (R15's exception)
};

// The day (R15).
struct Sun {
  double hour;  // solar hour the run starts at
  double day_of_year;
  double latitude;
  std::optional<double> facing;  // the face's compass bearing; absent: due north (every v1 map)
};

struct TrackIndex;  // query.hpp: the spatial hash `nearest_track_point` reads

// THE PISTE, open: from the start line (s = 0) to the finish (s = length). `index` is built
// the first time the piste is asked where a point is nearest (query.hpp), off the stations'
// x and z, which nothing moves after the walk (only `y` is written later).
struct Track {
  std::vector<TrackPoint> points;
  double length = 0;
  mutable std::shared_ptr<const TrackIndex> index;
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
  std::optional<PisteGrade> grade;             // the grade built to (R23); absent on v1
  std::optional<ge::core::Heightfield> crust;  // the region's wind crust (R21), if it lays one

  double ground_at(double x, double z) const { return ge::core::sample_field(ground, x, z); }
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
  std::optional<int> only;  // build this one attempt alone and throw its refusal
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
