// THE CLASSIC ROW ON THE ENGINE'S MOVER, ITS OWN BITS AND ITS RULES: every recorded scenario's
// script run through the row from its start over its fixture world (the run of
// plugins/shooter/row_recording.hpp) hashed line by line (FNV-1a 64 over each line's position and
// velocity bits, in double, and its discrete fields) against tests/golden/shooter_row.txt, which
// this test writes itself under GE_GOLDEN_WRITE=1 (a deliberate change to the row, the mover or
// the swimmer rewrites it, and the PR says why); the same runs twice agree; and the rules of
// the row: its numbers come from the settings, a long command runs as its halves, a strafe
// jumper gains speed in the air and a straight hop does not.
//
// Golden rows: scenario index, line count, digest (hex words).
//
// ge:summary Tests: the engine mover's classic row, its bits on every fixture script, its rules
// ge:keywords test row golden digest determinism classic_rows row_command split strafe-jump
// ge:keywords fixture scripted-movement
// ge:skill shooter move testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "shooter/row.hpp"
#include "shooter/row_recording.hpp"

namespace sh = ge::shooter;
namespace mv = ge::move;

namespace {

constexpr const char* kGolden = "tests/golden/shooter_row.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

const sh::Recording& recording() {
  static const sh::Recording rec = [] {
    std::ifstream f(ge::test::root_path("tests/golden/shooter_recording.txt"));
    std::stringstream ss;
    ss << f.rdbuf();
    sh::Recording r;
    std::string error;
    if (!sh::parse_recording(ss.str(), r, error)) std::fprintf(stderr, "%s\n", error.c_str());
    return r;
  }();
  return rec;
}

struct Fnv {
  uint64_t h = 0xcbf29ce484222325ull;
  void add(const void* p, size_t n) {
    const auto* b = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 0x100000001b3ull;
  }
  void add_int(int v) { add(&v, sizeof v); }
  void add_double(double v) { add(&v, sizeof v); }
};

// The flat scene as a world of one model, and a body standing on it.
struct Flat {
  sh::ModelData data = sh::build_brush_model(recording().scenes[0].brushes);
  sh::BrushModel model = data.model();
  sh::RowWorld world;
  Flat() {
    world.hulls = {&model, nullptr, 1};
    sh::build_row_world(world);
  }
};

sh::RowBody standing(const sh::ClassicRows& rows, const sh::RowWorld& world) {
  sh::MoveState s;
  s.origin = {0, 0, 24};
  return sh::row_body(s, rows, world);
}

uint64_t digest_of(const sh::RowBody& b, Fnv& f) {
  for (double v :
       {b.body.pos.x, b.body.pos.y, b.body.pos.z, b.body.vel.x, b.body.vel.y, b.body.vel.z})
    f.add_double(v);
  f.add_int(b.body.grounded ? static_cast<int>(b.body.ground_solid) : -1);
  f.add_int(b.waterlevel);
  f.add_int(b.watertype);
  f.add_double(b.swim.jumping);
  return f.h;
}

// The scenario's script through the row from its start: the digest of every line.
uint64_t run_digest(const sh::RecordedScenario& sc, size_t& lines) {
  const sh::Recording& rec = recording();
  sh::ModelData world = sh::build_brush_model(rec.scenes[static_cast<size_t>(sc.scene)].brushes);
  std::vector<sh::ModelData> boxes;
  for (const sh::RecordedBox& b : sc.boxes) {
    sh::Brush brush;
    brush.planes = {{{1, 0, 0}, b.maxs.x},   {{-1, 0, 0}, -b.mins.x}, {{0, 1, 0}, b.maxs.y},
                    {{0, -1, 0}, -b.mins.y}, {{0, 0, 1}, b.maxs.z},   {{0, 0, -1}, -b.mins.z}};
    boxes.push_back(sh::build_brush_model({brush}));
  }
  std::vector<sh::BrushModel> models = {world.model()};
  std::vector<sh::Vec3f> origins = {{}};
  for (size_t i = 0; i < boxes.size(); i++) {
    models.push_back(boxes[i].model());
    origins.push_back(sc.boxes[i].origin);
  }
  sh::RowWorld w;
  w.hulls = {models.data(), origins.data(), static_cast<int>(models.size())};
  sh::build_row_world(w);
  const sh::ClassicRows rows = sh::classic_rows(sc.settings, sc.start.maxspeed);
  sh::RowBody b = sh::row_body(sc.start, rows, w);
  Fnv f;
  for (const sh::RecordedLine& line : sc.lines) {
    sh::row_command(b, line.cmd, rows, w);
    digest_of(b, f);
    f.add_int(-99);
  }
  lines = sc.lines.size();
  return f.h;
}

}  // namespace

TEST(every_fixture_script_matches_its_own_bits) {
  const sh::Recording& rec = recording();
  std::vector<std::vector<uint64_t>> got;
  for (size_t i = 0; i < rec.scenarios.size(); i++) {
    size_t n = 0, m = 0;
    const uint64_t a = run_digest(rec.scenarios[i], n);
    const uint64_t b = run_digest(rec.scenarios[i], m);
    CHECK_MSG(a == b, rec.scenarios[i].name +
                          " differs between two runs: the row reads state from outside its "
                          "arguments (load the determinism skill)");
    got.push_back({i, n, a});
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# plugins/shooter/row.hpp's own record: scenario, lines, digest of every line "
           "(tests/shooter_row_test.cpp)\n";
    for (const auto& row : got) {
      char line[64];
      std::snprintf(
          line, sizeof line, "%llx %llx %016llx\n", static_cast<unsigned long long>(row[0]),
          static_cast<unsigned long long>(row[1]), static_cast<unsigned long long>(row[2]));
      out << line;
    }
    std::printf("wrote %s (%zu rows)\n", kGolden, got.size());
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == got.size(), std::string(kGolden) +
                                           " has the wrong row count: rewrite it with "
                                           "GE_GOLDEN_WRITE=1 if the row changed on purpose");
  for (size_t i = 0; i < rows.size() && i < got.size(); i++) {
    CHECK_MSG(rows[i] == got[i],
              rec.scenarios[i].name +
                  " moved off its golden bits: if the change is meant, check "
                  "tests/shooter_row_recording_test.cpp still passes, then rewrite with "
                  "GE_GOLDEN_WRITE=1 and say why in the PR");
  }
}

TEST(the_rows_take_the_settings) {
  sh::MoveSettings set;
  set.gravity = 600;
  set.friction = 6;
  set.accelerate = 12;
  set.bunnyspeedcap = 1.2f;
  set.spectatormaxspeed = 450;
  set.waterfriction = 2;
  const sh::ClassicRows r = sh::classic_rows(set, 300, 0.5f);
  CHECK(r.walk.gravity == 300);  // the server's times the player's factor
  CHECK(r.walk.friction == 6 && r.walk.accelerate == 12 && r.walk.air_accelerate == 12);
  CHECK(r.walk.max_speed == 300);
  CHECK(r.walk.air_max_speed == 300 * static_cast<double>(1.2f));
  CHECK(r.walk.air_speed_cap == 30 && r.walk.gravity_lead == 1 && r.walk.land_clip_speed == 300);
  CHECK(r.fly.stop_speed == 0 && r.fly.friction == 4);
  CHECK(r.spectator.max_speed == 450 && r.spectator.friction == 9);
  CHECK(r.swim.friction == 2 && r.swim.run_speed == 300 && r.swim.along_wish);
  CHECK(r.swim.idle_sink == 60 * 0.7);
}

TEST(a_long_command_runs_as_its_halves) {
  Flat flat;
  const sh::ClassicRows rows = sh::classic_rows({});
  sh::MoveCmd cmd;
  cmd.forward = 400;
  cmd.angles = {0, 30, 0};
  sh::RowBody a = standing(rows, flat.world), b = standing(rows, flat.world);
  cmd.msec = 75;
  sh::row_command(a, cmd, rows, flat.world);
  cmd.msec = 37;
  sh::row_command(b, cmd, rows, flat.world);
  sh::row_command(b, cmd, rows, flat.world);
  CHECK(a.body.pos.x == b.body.pos.x && a.body.pos.z == b.body.pos.z);
  CHECK(a.body.vel.x == b.body.vel.x && a.body.vel.z == b.body.vel.z);
}

TEST(a_strafe_jumper_gains_speed_and_a_straight_hop_does_not) {
  Flat flat;
  const sh::ClassicRows rows = sh::classic_rows({});
  auto hop = [&](bool strafe) {
    sh::RowBody b = standing(rows, flat.world);
    sh::MoveCmd cmd;
    cmd.msec = 13;
    cmd.forward = 400;
    for (int i = 0; i < 60; i++) sh::row_command(b, cmd, rows, flat.world);  // up to speed
    double yaw = 0;
    for (int i = 0; i < 300; i++) {
      cmd.buttons = b.body.grounded && !b.body.jump_held ? sh::kButtonJump : 0;
      if (strafe) {  // turn right while strafing right: the classic air gain
        cmd.forward = 0;
        cmd.side = 400;
        yaw -= 1.2;
      }
      cmd.angles = {0, static_cast<float>(yaw), 0};
      sh::row_command(b, cmd, rows, flat.world);
    }
    return mv::flat_length(b.body.vel);
  };
  const double straight = hop(false), strafed = hop(true);
  CHECK_MSG(straight <= 320.5, "a straight hop must not gain: " + std::to_string(straight));
  CHECK_MSG(strafed > 400, "a strafe jumper must gain: " + std::to_string(strafed));
}
