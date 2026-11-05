// Tests: the sequel's player move (plugins/shooter/sequel_move.hpp) over brush worlds written as
// version 38 maps. A player on a floor stays put to the eighth; running caps at 300; the jump
// adds 270 and must be pressed afresh; an 18-unit step is climbed and a 24-unit one is not; a
// steep slope is no ground; ducking caps the run at 100 and lowers the view; a fast landing
// pauses the jump; water sinks a still swimmer and lifts one pressing up; the water jump climbs a
// bank; a ladder is climbed; a spectator flies through a wall; a changed state finds a free start;
// and a scripted run over every part holds golden (tests/golden/shooter_sequel_move.txt; rewrite it
// with GE_GOLDEN_WRITE=1 when the move deliberately changes, and say why).
//
// ge:summary Test: the sequel's player move over brush worlds: run, jump, steps, duck, swim, golden
// ge:keywords test sequel sequel-move brush-world run jump step duck water ladder spectator golden
// ge:skill shooter testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "golden.hpp"
#include "shooter/sequel_move.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;

constexpr const char* kGolden = "tests/golden/shooter_sequel_move.txt";

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1,
                   int32_t contents = sh::kSqSolid) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, contents, "world/wall");
}

// The course, along +x from the origin: a floor; a step of 18 at x 256 and one of 24 more at
// x 512; a pool (x -512..-256, 96 deep) with a bank; a steep ramp and a gentle one to the side
// (y 256..512); a ladder on a wall at y -256; walls round it all. A player at rest on a floor
// stands an eighth above it (24.125 over a floor at 0): resting on the plane is touching it, which
// the trace calls solid, so the packing nudges the origin up.
br::BrushWorld course() {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-256, -512, -16, 1024, 512, 0));     // the floor
  m.push_back(box(256, -128, 0, 1024, 128, 18));       // a step of 18
  m.push_back(box(512, -128, 18, 1024, 128, 42));      // and one of 24 on it
  m.push_back(box(-512, -512, -112, -256, 512, -96));  // the pool's floor
  m.push_back(box(-512, -512, -96, -256, 512, -8, sh::kSqWater));
  m.push_back(box(-576, -512, -112, -512, 512, 256));  // the walls round it all
  m.push_back(box(1024, -512, -112, 1040, 512, 256));
  m.push_back(box(-576, 512, -112, 1040, 528, 256));
  m.push_back(box(-576, -528, -112, 1040, -512, 256));
  br::WorldBrush steep = box(-128, 256, 0, 128, 512, 128);
  const float s[3] = {0, -2, 1};  // rises 2 in 1 toward +y
  br::add_side(&steep, s, -512.0f / 2.2360679f * 1.0f);
  m.push_back(steep);
  br::WorldBrush gentle = box(256, 256, 0, 512, 512, 64);
  const float g[3] = {0, -1, 4};  // rises 1 in 4
  br::add_side(&gentle, g, -256.0f / 4.1231055f);
  m.push_back(gentle);
  m.push_back(box(-64, -272, 0, 64, -256, 256, sh::kSqSolid | sh::kSqLadder));
  return w;
}

struct World {
  br::Map map;
  std::unique_ptr<br::BrushTracer> tracer;
  sh::SequelBrushWorld brushes;

  World() {
    std::vector<uint8_t> bytes;
    std::string error;
    CHECK_MSG(br::write_brush_world(course(), &bytes, &error), error);
    CHECK_MSG(br::read_bsp(bytes, &map, &error), error);
    tracer = std::make_unique<br::BrushTracer>(map);
    brushes.tracer = tracer.get();
  }
};

sh::SequelState at(float x, float y, float z) {
  sh::SequelState s;
  s.origin[0] = int16_t(x * 8);
  s.origin[1] = int16_t(y * 8);
  s.origin[2] = int16_t(z * 8);
  return s;
}

int16_t yaw(float deg) { return int16_t(int(deg * 65536 / 360)); }

struct Run {
  World& w;
  sh::SequelMove pm;
  explicit Run(World& world, sh::SequelState s) : w(world) { pm.s = s; }
  void step(sh::SequelCmd c, int n = 1) {
    for (int i = 0; i < n; ++i) {
      pm.cmd = c;
      sh::sequel_move(pm, w.brushes.world());
      pm.changed = false;
    }
  }
  float x() const { return pm.s.origin[0] * 0.125f; }
  float y() const { return pm.s.origin[1] * 0.125f; }
  float z() const { return pm.s.origin[2] * 0.125f; }
  float vx() const { return pm.s.velocity[0] * 0.125f; }
  float vz() const { return pm.s.velocity[2] * 0.125f; }
};

sh::SequelCmd cmd(int16_t forward = 0, int16_t side = 0, int16_t up = 0, int16_t yaw_ = 0,
                  int16_t pitch = 0, uint8_t msec = 16) {
  sh::SequelCmd c;
  c.msec = msec;
  c.forward = forward;
  c.side = side;
  c.up = up;
  c.angles[0] = pitch;
  c.angles[1] = yaw_;
  return c;
}

TEST(a_player_on_the_floor_stays_put_to_the_eighth) {
  World w;
  Run r(w, at(0, 0, 24.125f));
  r.step(cmd(), 30);
  CHECK(r.pm.s.origin[0] == 0 && r.pm.s.origin[2] == 193);
  CHECK(r.pm.s.velocity[0] == 0 && r.pm.s.velocity[2] == 0);
  CHECK(r.pm.s.flags & sh::kSqOnGround);
  CHECK(r.pm.ground_entity == 0 && r.pm.view_height == 22 && r.pm.water_level == 0);
}

TEST(running_caps_at_300) {
  World w;
  Run r(w, at(-200, 0, 24.125f));
  r.step(cmd(400), 40);
  CHECK(r.vx() > 290 && r.vx() <= 300);
  CHECK(r.z() == 24.125f);
  // Friction brings it to rest.
  r.step(cmd(), 60);
  CHECK(r.pm.s.velocity[0] == 0);
}

TEST(the_jump_adds_270_and_must_be_pressed_afresh) {
  World w;
  Run r(w, at(0, 0, 24.125f));
  r.step(cmd(0, 0, 400));
  CHECK(r.pm.s.velocity[2] == int16_t(int((270.0f - 800.0f * 0.016f) * 8)));
  CHECK((r.pm.s.flags & sh::kSqJumpHeld) && !(r.pm.s.flags & sh::kSqOnGround));
  float top = 0;
  for (int i = 0; i < 80; ++i) {
    r.step(cmd(0, 0, 400));
    if (r.z() > top) top = r.z();
  }
  CHECK(top > 24 + 40 && top < 24 + 48);                        // 270² / (2 × 800) = 45.6
  CHECK((r.pm.s.flags & sh::kSqOnGround) && r.z() == 24.125f);  // landed, held: no second jump
  r.step(cmd());
  r.step(cmd(0, 0, 400));
  CHECK(r.vz() > 200);
}

TEST(an_18_unit_step_is_climbed_and_a_24_unit_one_is_not) {
  World w;
  Run r(w, at(150, 0, 24.125f));
  r.step(cmd(400), 40);
  CHECK(r.x() > 300 && r.z() == 42.125f);
  r.step(cmd(400), 80);
  CHECK(r.x() < 512 - 16 + 1 && r.z() == 42.125f);
}

TEST(a_steep_slope_is_no_ground_and_a_gentle_one_is) {
  World w;
  // Above the gentle ramp's middle (z = (y − 256) / 4 under it).
  Run g(w, at(384, 384, 24 + 32 + 4));
  g.step(cmd(), 30);
  CHECK(g.pm.s.flags & sh::kSqOnGround);
  CHECK(g.pm.ground_entity == 0);
  // On the steep one it slides down toward −y and never stands.
  Run s(w, at(0, 290, 132));  // over the slope (z = 2y − 512 under the box's high side)
  bool stood = false;
  for (int i = 0; i < 40; ++i) {
    s.step(cmd());
    if ((s.pm.s.flags & sh::kSqOnGround) && s.z() > 30) stood = true;
  }
  CHECK(!stood);
  CHECK(s.y() < 256 && s.z() == 24.125f);  // slid off onto the floor
}

TEST(ducking_caps_the_run_at_100_and_lowers_the_view) {
  World w;
  Run r(w, at(-200, 0, 24.125f));
  r.step(cmd(400, 0, -400), 60);
  CHECK(r.pm.s.flags & sh::kSqDucked);
  CHECK(r.pm.view_height == -2 && r.pm.maxs.z == 4);
  CHECK(r.vx() > 95 && r.vx() <= 100);
  r.step(cmd(), 2);
  CHECK(!(r.pm.s.flags & sh::kSqDucked) && r.pm.view_height == 22);
}

// A landing faster than 200 down pauses the jump (25 × 8 ms past 400 down, 18 past 200). The
// pause starts when a command begins within 0.25 of the floor still falling; a command whose
// slide reaches the floor clips the fall away first.
TEST(a_fast_landing_pauses_the_jump) {
  World w;
  sh::SequelState s = at(0, 0, 24.25f);
  s.velocity[2] = -500 * 8;
  Run r(w, s);
  r.step(cmd());
  CHECK((r.pm.s.flags & sh::kSqTimeLand) && (r.pm.s.flags & sh::kSqOnGround));
  CHECK(r.pm.s.time == 23);  // 25, less this command's 16 ms
  r.step(cmd(0, 0, 400));
  CHECK(r.pm.s.velocity[2] == 0);  // no jump while the pause runs
  r.step(cmd(), 12);
  r.step(cmd(0, 0, 400));
  CHECK(r.vz() > 200);
}

TEST(water_sinks_a_still_swimmer_and_lifts_one_pressing_up) {
  World w;
  Run r(w, at(-384, 0, -60));
  r.step(cmd());
  CHECK(r.pm.water_level == 3 && r.pm.water_type == sh::kSqWater);
  r.step(cmd(), 40);
  CHECK(r.vz() < 0 && r.vz() > -40);
  Run u(w, at(-384, 0, -60));
  u.step(cmd(0, 0, 400), 10);
  CHECK(u.vz() > 100 && u.z() > -50);
}

TEST(the_water_jump_climbs_the_bank) {
  World w;
  // At the near edge, chest deep, facing the floor at x > −256 (its top 8 above the water).
  Run r(w, at(-280, 0, -16));
  r.step(cmd(400), 1);
  bool jumped = false;
  for (int i = 0; i < 60; ++i) {
    r.step(cmd(400));
    if (r.pm.s.flags & sh::kSqTimeWaterJump) jumped = true;
  }
  CHECK(jumped);
  CHECK(r.x() > -240 && r.z() == 24.125f);
}

TEST(a_ladder_is_climbed) {
  World w;
  Run r(w, at(0, -200, 24));
  r.step(cmd(400, 0, 0, yaw(-90), int16_t(-30 * 65536 / 360)), 80);
  CHECK(r.z() > 100);
}

TEST(a_spectator_flies_through_a_wall) {
  World w;
  sh::SequelState s = at(-200, 400, 60);  // the steep ramp's solid is ahead along +x
  s.type = sh::SequelType::spectator;
  Run r(w, s);
  r.step(cmd(400), 90);
  CHECK(r.x() > 150 && r.z() == 60);
}

TEST(a_changed_state_finds_a_free_start) {
  World w;
  Run r(w, at(0, 0, 24));  // on the floor's plane: touching it, so solid
  r.pm.changed = true;
  r.step(cmd());
  CHECK(r.pm.s.origin[2] == 193 && (r.pm.s.flags & sh::kSqOnGround));
}

TEST(a_scripted_run_holds_golden) {
  World w;
  Run r(w, at(-100, 0, 24.125f));
  std::string table = "# origin velocity flags time per 25 commands (shooter_sequel_move_test)\n";
  std::vector<std::vector<uint64_t>> rows;
  for (int i = 0; i < 600; ++i) {
    const int phase = i / 60;
    const auto turn = int16_t(i * 97);
    sh::SequelCmd c = cmd(phase % 3 == 0 ? 400 : -200, phase % 2 ? 300 : -100,
                          i % 37 < 3 ? 400 : (phase == 4 ? -400 : 0), turn, int16_t((i % 50) * 120),
                          uint8_t(8 + i % 23));
    r.step(c);
    if (i % 25 == 24) {
      const sh::SequelState& s = r.pm.s;
      std::vector<uint64_t> row;
      for (int k = 0; k < 3; ++k) row.push_back(uint16_t(s.origin[k]));
      for (int k = 0; k < 3; ++k) row.push_back(uint16_t(s.velocity[k]));
      row.push_back(s.flags);
      row.push_back(s.time);
      char line[128];
      std::snprintf(
          line, sizeof line, "%04llx %04llx %04llx %04llx %04llx %04llx %02llx %02llx\n",
          static_cast<unsigned long long>(row[0]), static_cast<unsigned long long>(row[1]),
          static_cast<unsigned long long>(row[2]), static_cast<unsigned long long>(row[3]),
          static_cast<unsigned long long>(row[4]), static_cast<unsigned long long>(row[5]),
          static_cast<unsigned long long>(row[6]), static_cast<unsigned long long>(row[7]));
      table += line;
      rows.push_back(row);
    }
  }
  const char* write = std::getenv("GE_GOLDEN_WRITE");
  if (write && *write) {
    std::ofstream(ge::test::root_path(kGolden)) << table;
    std::printf("  wrote %s\n", kGolden);
    return;
  }
  const auto golden = ge::test::golden_rows(kGolden);
  CHECK(golden.size() == rows.size());
  for (size_t k = 0; k < golden.size() && k < rows.size(); ++k) CHECK(golden[k] == rows[k]);
}

}  // namespace
