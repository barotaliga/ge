// LAB `se-remix-ladder`: QUAKE II'S LADDERS CLIMBED BY QUAKE'S MOVE. Under classic remix a Quake II
// map plays on Quake's rules and Quake's move; its ladders (brushes with the ladder bit) are
// climbed by the rule both moves share (plugins/shooter/ladder.hpp), so the map's high ground can
// be reached. For each ladder of a real map (`--q2=<install> --q2-map=<name>`) the player is put
// on the floor in front of it, facing it, and walks into it looking up for three seconds, under
// Quake's move and under Quake II's own; the smoke run (no install) climbs a made-up wall.
//
// What it shows: a table of every ladder (where, how tall, how high each move climbed in three
// seconds, at what speed) and a chart of the feet's height over time on the first ladder Quake II's
// move climbs, both
// moves. Read it for the climb: both lines rise at 200 u/s to the ladder's top and go on over it;
// a Quake line flat at the floor would be the map unplayable. Deterministic: the digest holds the
// heights.
//
// ge:summary Lab: Quake's move climbing a Quake II map's ladders beside Quake II's own move
// ge:keywords lab se-remix-ladder ladder climb remix-ladder classic-remix q2-map version-38
// ge:skill shooter labs
// ge:test tests/shooter_ladder_test.cpp
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "classic/q2_move.hpp"
#include "classicenhance/profile.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/ladder.hpp"
#include "shooter/pmove.hpp"
#include "shooter/world.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"

namespace {

namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace ce = ge::classicenhance;
namespace nt = ge::core::num;

constexpr int kMsec = 10;          // a command every 10 ms
constexpr double kClimbS = 3;      // walking into the ladder, looking up
constexpr float kLookUp = -30;     // the view's pitch (degrees, up negative)
constexpr int kMostLadders = 12;   // the table's rows at most
constexpr float kHallHalf = 1024;  // the smoke run's hall

// A ladder's box, from its brush's axial sides.
struct Box {
  float lo[3] = {-1e9f, -1e9f, -1e9f}, hi[3] = {1e9f, 1e9f, 1e9f};
};

Box brush_box(const br::Map& map, const br::Brush& b) {
  Box box;
  for (uint32_t i = 0; i < b.sides; ++i) {
    const br::Plane& p = map.planes[map.brush_sides[b.first_side + i].plane];
    for (int a = 0; a < 3; ++a) {
      if (p.n[a] == 1) box.hi[a] = std::min(box.hi[a], p.d);
      if (p.n[a] == -1) box.lo[a] = std::max(box.lo[a], -p.d);
    }
  }
  return box;
}

// Where a player stands in front of a ladder, facing it: on the floor, the box one unit off its
// widest face, in the middle of it. False when neither side has room.
struct Start {
  sh::Vec3f origin;
  float yaw = 0;
};

bool start_at(const sh::MoveWorld& world, const Box& b, Start* out) {
  const int thin = (b.hi[0] - b.lo[0]) <= (b.hi[1] - b.lo[1]) ? 0 : 1, along = 1 - thin;
  for (const int sgn : {1, -1}) {
    sh::Vec3f p;
    p[thin] = (sgn > 0 ? b.hi[thin] : b.lo[thin]) + float(sgn) * 17;
    p[along] = (b.lo[along] + b.hi[along]) * 0.5f;
    for (int k = 0; b.lo[2] + 25 + float(k * 8) < b.hi[2]; ++k) {
      p.z = b.lo[2] + 25 + float(k * 8);
      if (!sh::box_free(world, p)) continue;
      const sh::HullTrace down = sh::player_trace(world, p, {p.x, p.y, p.z - 256});
      if (down.fraction == 1 || down.plane.normal.z < 0.7f) break;
      out->origin = down.endpos;
      out->yaw = thin == 0 ? (sgn > 0 ? 180.0f : 0.0f) : (sgn > 0 ? 270.0f : 90.0f);
      return true;
    }
  }
  return false;
}

struct Climb {
  std::vector<double> t, z;  // the feet over the start, by time
  double rise = 0;           // the most the feet rose
  double speed = 0;          // u/s over the first second on the rungs
};

void note(Climb& c, double t, double rise) {
  c.t.push_back(t);
  c.z.push_back(rise);
  c.rise = std::max(c.rise, rise);
  if (t <= 1.0 + 1e-9) c.speed = rise;
}

Climb quake_climb(const sh::MoveWorld& world, const Start& s0) {
  const sh::MoveSettings set;
  sh::MoveState s;
  s.origin = s0.origin;
  s.angles = {0, s0.yaw, 0};
  Climb c;
  for (int i = 1; i * kMsec * 0.001 <= kClimbS + 1e-9; ++i) {
    sh::MoveCmd cmd;
    cmd.msec = kMsec;
    cmd.angles = {kLookUp, s0.yaw, 0};
    cmd.forward = 400;
    sh::player_move(s, cmd, set, world);
    note(c, i * kMsec * 0.001, double(s.origin.z - s0.origin.z));
  }
  return c;
}

int16_t short_angle(float degrees) {
  return int16_t(int(std::floor(double(degrees) * 65536 / 360 + 0.5)) & 0xffff);
}

Climb q2_climb(br::BrushTracer& tracer, const Start& s0) {
  ge::classic::Q2BrushWorld brushes;
  brushes.tracer = &tracer;
  ge::classic::Q2Move pm;
  for (int a = 0; a < 3; ++a) pm.s.origin[a] = int16_t(std::lround(s0.origin[a] * 8));
  pm.changed = true;
  const float z0 = float(pm.s.origin[2]) * 0.125f;
  Climb c;
  for (int i = 1; i * kMsec * 0.001 <= kClimbS + 1e-9; ++i) {
    ge::classic::Q2Cmd cmd;
    cmd.msec = kMsec;
    cmd.angles[0] = short_angle(kLookUp);
    cmd.angles[1] = short_angle(s0.yaw);
    cmd.forward = 400;
    pm.cmd = cmd;
    ge::classic::q2_move(pm, brushes.world());
    pm.changed = false;
    note(c, i * kMsec * 0.001, double(float(pm.s.origin[2]) * 0.125f - z0));
  }
  return c;
}

// The smoke run's made-up hall: a floor, a ceiling and one ladder 192 high across +y.
bool made_up_hall(br::Map* map, std::string* error) {
  br::BrushWorld w;
  w.models.resize(1);
  const auto add = [&](float x0, float y0, float z0, float x1, float y1, float z1, int32_t c) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    w.models[0].push_back(br::box_brush(lo, hi, c, "world/floor"));
  };
  add(-kHallHalf, -kHallHalf, -16, kHallHalf, kHallHalf, 0, sh::kSqSolid);
  add(-kHallHalf, -kHallHalf, 512, kHallHalf, kHallHalf, 528, sh::kSqSolid);
  add(-64, 128, 0, 64, 136, 192, sh::kSqSolid | sh::kSqLadder);
  std::vector<uint8_t> bytes;
  return br::write_brush_world(w, &bytes, error) && br::read_bsp(bytes, map, error);
}

void run(gl::Lab& lab) {
  const std::string q2 = lab.arg("q2"), name = lab.arg("q2-map");
  std::unique_ptr<cl::LoadedMap> loaded;
  br::Map hall;
  const br::Map* map = nullptr;
  std::string error;
  std::string where = "the made-up hall";
  if (q2.empty()) {
    if (!made_up_hall(&hall, &error))
      return lab.fail(error, "next: the hall's brushes; skill: brush.");
    map = &hall;
  } else {
    const ce::Profile* content = ce::find_profile("q2");
    cl::SearchPath files;
    if (!cl::open_game_dir(q2, "", files, error, *content))
      return lab.fail(error, "next: pass the install folder that holds baseq2; skill: shooter.");
    loaded = cl::load_named_map(files, name, error);
    if (!loaded) return lab.fail(error, "next: name a map the install holds with --q2-map.");
    map = &loaded->bsp.map;
    where = name;
  }
  br::BrushTracer tracer(*map);
  const br::Model& world_model = map->models[0];
  sh::MoveWorld world;
  world.solids[0] = sh::brushes_solid(
      tracer, 0, {}, {world_model.mins[0], world_model.mins[1], world_model.mins[2]},
      {world_model.maxs[0], world_model.maxs[1], world_model.maxs[2]});
  world.count = 1;

  gl::LabTable table{
      "Every ladder of " + where +
          ": Quake's move and Quake II's walking into it, "
          "looking up, for three seconds",
      {"ladder", "at", "tall", "Quake rise", "Quake u/s", "Quake II rise", "Quake II u/s"},
      {}};
  Climb first_q1, first_q2;
  int ladders = 0, climbed = 0, placed = 0, misses = 0;
  // A ladder is often several brushes stacked; each brush is tried, the first few placed shown.
  for (size_t i = 0; i < map->brushes.size() && placed < kMostLadders; ++i) {
    const br::Brush& b = map->brushes[i];
    if ((b.contents & sh::kSqLadder) == 0) continue;
    ++ladders;
    const Box box = brush_box(*map, b);
    Start s;
    if (!start_at(world, box, &s)) continue;
    const Climb q1 = quake_climb(world, s);
    const Climb q2c = q2_climb(tracer, s);
    if (first_q2.t.empty() && q2c.rise > 32) {  // the chart's: the first Quake II climbs
      first_q1 = q1;
      first_q2 = q2c;
    }
    if (q1.rise > 32) ++climbed;
    if (q2c.rise > 32 && q1.rise < q2c.rise - 16) ++misses;  // Quake II climbs it, Quake not
    const std::string id = std::to_string(i);
    table.rows.push_back({id,
                          nt::to_fixed(s.origin.x, 0) + " " + nt::to_fixed(s.origin.y, 0) + " " +
                              nt::to_fixed(s.origin.z, 0),
                          nt::to_fixed(box.hi[2] - box.lo[2], 0), nt::to_fixed(q1.rise, 1),
                          nt::to_fixed(q1.speed, 1), nt::to_fixed(q2c.rise, 1),
                          nt::to_fixed(q2c.speed, 1)});
    if (placed < 3) {
      lab.metric("ladder" + std::to_string(placed) + ".quake_rise", q1.rise, "u");
      lab.metric("ladder" + std::to_string(placed) + ".q2_rise", q2c.rise, "u");
    }
    ++placed;
  }
  lab.metric("ladder_brushes", ladders, "");
  lab.metric("placed", placed, "");
  lab.metric("climbed_by_quake", climbed, "");
  lab.metric("quake_misses", misses, "");  // ladders Quake II's move climbs and Quake's not: 0
  lab.table(table);
  if (first_q2.t.empty()) return lab.note(where + " has no ladder a player climbs from its floor.");

  if (lab.wants("climb")) {
    gl::LineChart c;
    c.spikes = false;
    c.frame = {"A LADDER OF " + where,
               "walking into it looking up, each move",
               "t (s)",
               "feet over the floor (units)",
               960,
               420};
    c.traces.push_back({"Quake's move (remix)", first_q1.t, first_q1.z});
    c.traces.push_back({"Quake II's move", first_q2.t, first_q2.z});
    const gl::Chart ch = gl::line_chart(c);
    lab.shot({"climb",
              ch.title,
              "The player stands on the floor facing the ladder and walks into it with the view "
              "30 degrees up. On the rungs the climb is 200 u/s, gravity held; at the top the "
              "player walks off onto the ledge. Quake's move takes hold by the rule Quake II's "
              "move runs, so its line follows Quake II's.",
              "both lines rise together; a flat Quake line would mean the ladder is not climbed",
              {{"msec", std::to_string(kMsec)}, {"map", where}}},
             gl::to_picture(ch.plot));
  }
}

const gl::LabSpec kSpec{
    "se-remix-ladder",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/shooter/ladder.hpp: Quake's move on a Quake II map's ladders under classic remix",
    "shooter",
    "Does Quake's move climb every ladder of a Quake II map as Quake II's own move does?",
    {{"q2", "", "Quake II's install (the folder holding baseq2; empty: the made-up hall)"},
     {"q2-map", "q2dm1", "the Quake II map whose ladders are climbed"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
