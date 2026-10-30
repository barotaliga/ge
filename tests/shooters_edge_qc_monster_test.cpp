// Tests: the monster moves (games/shooters_edge/server/bi_monster.cpp): the angle grid and
// changeyaw's short way round at its speed, walkmove on the floor, into a wall and off the ledge,
// checkbottom on the floor and in the air, a flyer's height correction toward its enemy, and
// movetogoal chasing a goal round the arena's dividing wall through the doorway, its path held to
// the bit by tests/golden/shooters_edge_qc_monster.txt (the coins it draws come from the server's
// seeded generator).
//
// ge:summary Tests: walkmove, checkbottom, changeyaw and movetogoal, with movetogoal's golden path
// ge:keywords test qc monster walkmove movetogoal checkbottom changeyaw wrap_angle golden
// ge:skill progs shooter testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "shooter/entity.hpp"
#include "shooters_edge/server/builtins.hpp"
#include "shooters_edge_qc_fixture.hpp"

using qcfix::ent;
using qcfix::Fixture;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

constexpr const char* kGolden = "tests/golden/shooters_edge_qc_monster.txt";

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

// A walking monster standing at `at` (its origin 24 above its feet), turning 20 a call.
int monster(Fixture& fx, sh::Vec3f at, int flags = sh::kFlagOnGround) {
  const int e = fx.spawn();
  fx.call(4, {ent(e), sh::Vec3f{-16, -16, -24}, sh::Vec3f{16, 16, 40}});
  fx.call(2, {ent(e), at});
  sh::Edict& ed = fx.s->sv.edicts[static_cast<size_t>(e)];
  ed.movetype = sh::kMovetypeStep;
  ed.solid = sh::kSolidSlidebox;
  ed.flags = flags;
  sh::link_edict(fx.s->sv, e, false);
  fx.call(47, {ent(0)});  // any call: the loop's fields mirrored in
  fx.set_f(e, pg::kFYawSpeed, 20);
  return e;
}

}  // namespace

TEST(angles_wrap_on_the_16_bit_grid) {
  CHECK(sv::wrap_angle(-90) == 270.0f);
  CHECK(sv::wrap_angle(360) == 0.0f);
  CHECK(sv::wrap_angle(45) == 45.0f);
  const float step = static_cast<float>(360.0 / 65536.0);
  CHECK(sv::wrap_angle(step * 1.5f) == step);
}

TEST(changeyaw_turns_the_short_way_at_its_speed) {
  // Angles on the grid (multiples of 360 / 65536) so the sums are exact.
  CHECK(sv::turned_yaw(22.5f, 337.5f, 90) == 337.5f);
  CHECK(sv::turned_yaw(22.5f, 337.5f, 11.25f) == 11.25f);
  CHECK(sv::turned_yaw(337.5f, 22.5f, 90) == 22.5f);
  CHECK(sv::turned_yaw(90, 90, 5) == 90.0f);
  CHECK(sv::turned_yaw(0, 180, 22.5f) == 337.5f);  // exactly opposite: the negative way
  Fixture fx;
  const int m = monster(fx, {-300, -300, 24});
  fx.set_f(m, pg::kFIdealYaw, 90);
  fx.set_f(m, pg::kFYawSpeed, 22.5f);
  fx.call(49, {}, m);
  CHECK(fx.f(m, pg::kFAngles, 1) == 22.5f);
  CHECK(fx.s->sv.edicts[static_cast<size_t>(m)].angles.y == 22.5f);  // read back by the loop
}

TEST(walkmove_steps_on_the_floor_and_stops_at_walls_and_edges) {
  Fixture fx;
  const int m = monster(fx, {-300, 0, 24});
  fx.call(32, {0, 8}, m);
  CHECK(fx.rf == 1.0f);
  CHECK(std::fabs(fx.s->sv.edicts[static_cast<size_t>(m)].origin.x - -292) < 0.01f);
  // Into the west wall's face (x = -512): a box 16 wide cannot pass.
  fx.call(2, {ent(m), sh::Vec3f{-490, 0, 24}});
  fx.call(32, {180, 8}, m);
  CHECK(fx.rf == 0.0f);
  // Off the ledge's north edge (its top at z = 104, y = -384): no ground within 36.
  fx.call(2, {ent(m), sh::Vec3f{300, -440, 128}});
  fx.call(32, {90, 8}, m);
  CHECK(fx.rf == 1.0f);  // the box still wholly over the ledge
  fx.call(32, {90, 48}, m);
  CHECK(fx.rf == 0.0f);  // its north corners would hang over the floor, 104 down
  CHECK(std::fabs(fx.s->sv.edicts[static_cast<size_t>(m)].origin.y - -432) < 0.01f);
  // Not on the ground (and neither flying nor swimming): no move.
  fx.s->sv.edicts[static_cast<size_t>(m)].flags = 0;
  fx.call(32, {0, 8}, m);
  CHECK(fx.rf == 0.0f);
}

TEST(checkbottom_needs_ground_under_the_box) {
  Fixture fx;
  const int m = monster(fx, {-300, 0, 24});
  fx.call(40, {ent(m)});
  CHECK(fx.rf == 1.0f);
  fx.call(2, {ent(m), sh::Vec3f{-300, 0, 200}});
  fx.call(40, {ent(m)});
  CHECK(fx.rf == 0.0f);
  CHECK(!sv::check_bottom(*fx.s, m));
}

TEST(a_flyer_corrects_its_height_toward_its_enemy) {
  Fixture fx;
  const int target = monster(fx, {-100, 0, 24});
  const int m = monster(fx, {-300, 0, 150}, sh::kFlagFly);
  fx.set_f(m, pg::kFEnemy, 0);
  *pg::entity_field(fx.vm(), m, pg::kFEnemy) =
      static_cast<uint32_t>(pg::entity_value(fx.vm(), target));
  fx.call(32, {0, 8}, m);
  CHECK(fx.rf == 1.0f);
  const sh::Vec3f o = fx.s->sv.edicts[static_cast<size_t>(m)].origin;
  CHECK(std::fabs(o.z - 142) < 0.01f);  // more than 40 above it: 8 down
  CHECK(std::fabs(o.x - -292) < 0.01f);
}

TEST(movetogoal_chases_round_the_wall_and_holds_its_golden_path) {
  Fixture fx;
  const int goal = monster(fx, {300, 300, 24});
  const int m = monster(fx, {-300, -300, 24});
  *pg::entity_field(fx.vm(), m, pg::kFGoalentity) =
      static_cast<uint32_t>(pg::entity_value(fx.vm(), goal));
  std::vector<std::vector<uint64_t>> got;
  float closest = 1e9f;
  for (int i = 0; i < 400; i++) {
    fx.call(67, {8}, m);
    CHECK(!fx.fault);
    const sh::Vec3f o = fx.s->sv.edicts[static_cast<size_t>(m)].origin;
    const float d = std::sqrt((o.x - 300) * (o.x - 300) + (o.y - 300) * (o.y - 300));
    if (d < closest) closest = d;
    if (i % 25 == 24)
      got.push_back({static_cast<uint64_t>(i), pg::as_word(o.x), pg::as_word(o.y), pg::as_word(o.z),
                     pg::as_word(fx.f(m, pg::kFIdealYaw))});
  }
  std::printf("closest %.1f\n", closest);
  CHECK(closest < 120);  // it found the doorway and came round to the goal
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# games/shooters_edge/server/bi_monster.cpp's own record: call, origin x y z and "
           "ideal_yaw as float bits after it (tests/shooters_edge_qc_monster_test.cpp)\n";
    for (const auto& r : got) {
      char line[96];
      std::snprintf(line, sizeof line, "%llx %08llx %08llx %08llx %08llx\n",
                    static_cast<unsigned long long>(r[0]), static_cast<unsigned long long>(r[1]),
                    static_cast<unsigned long long>(r[2]), static_cast<unsigned long long>(r[3]),
                    static_cast<unsigned long long>(r[4]));
      out << line;
    }
    std::printf("wrote %s (%zu rows)\n", kGolden, got.size());
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == got.size(), std::string(kGolden) + " has the wrong row count");
  for (size_t i = 0; i < rows.size() && i < got.size(); i++)
    CHECK_MSG(rows[i] == got[i], "movetogoal moved off its golden path at call " +
                                     std::to_string(got[i][0]) +
                                     ": if the change is meant, rewrite with GE_GOLDEN_WRITE=1 "
                                     "and say why in the PR");
}

TEST(movetogoal_stops_when_its_enemy_is_within_reach) {
  Fixture fx;
  const int goal = monster(fx, {-260, 0, 24});
  const int m = monster(fx, {-300, 0, 24});
  *pg::entity_field(fx.vm(), m, pg::kFGoalentity) =
      static_cast<uint32_t>(pg::entity_value(fx.vm(), goal));
  *pg::entity_field(fx.vm(), m, pg::kFEnemy) =
      static_cast<uint32_t>(pg::entity_value(fx.vm(), goal));
  const sh::Vec3f before = fx.s->sv.edicts[static_cast<size_t>(m)].origin;
  fx.call(67, {16}, m);
  const sh::Vec3f after = fx.s->sv.edicts[static_cast<size_t>(m)].origin;
  CHECK(before.x == after.x && before.y == after.y);
}
