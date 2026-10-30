// Tests: a single-player program (crc 5927) on the mod server. The adapter mirrors the loop into
// that form's offsets, and the builtins that differ from the multiplayer form behave as its rules
// say: bprint and sprint take no level, aim is the autoaim under `sv_aim`, the Write family has no
// multicast destination, and the multiplayer server's own numbers (79..82) are not in the table.
// The program is the test mod's single-player stub (games/shooters_edge/server/test_mod.hpp).
//
// ge:summary Tests: a single-player program on the mod server: the mirror and its builtins
// ge:keywords test qc single-player campaign form layout mirror bprint sprint aim autoaim sv_aim
// ge:keywords multicast destination logfrag infokey stof
// ge:skill progs shooter testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "shooters_edge_qc_fixture.hpp"

using qcfix::ent;
using qcfix::Fixture;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

const pg::Layout& kSp = pg::kSinglePlayerLayout;

pg::Progs single_player_mod() {
  pg::Progs p;
  std::string error;
  CHECK_MSG(sv::build_single_player_stub(p, error), error);
  return p;
}

struct SoloFixture {
  pg::Progs mod = single_player_mod();
  Fixture fx{sv::kStubEntities, {}, &mod};

  // A solid box in the arena at `at` (setsize then setorigin link it), aimed at when `aimed`.
  int box(sh::Vec3f at, bool aimed) {
    const int e = fx.spawn();
    fx.in_call([&] {
      fx.set_f(e, kSp.f.solid, 2);  // a bounding box
      fx.set_f(e, kSp.f.takedamage, aimed ? 2.0f : 0.0f);
    });
    fx.call(4, {ent(e), sh::Vec3f{-16, -16, -24}, sh::Vec3f{16, 16, 32}});
    fx.call(2, {ent(e), at});
    return e;
  }
};

bool near(float a, float b) { return std::fabs(a - b) <= 1e-5f; }

}  // namespace

TEST(the_server_runs_a_single_player_program_in_its_form) {
  SoloFixture so;
  Fixture& fx = so.fx;
  CHECK_MSG(fx.loaded, fx.error);
  CHECK(fx.s->q.layout == &kSp);
  const int e = fx.spawn();
  fx.call(2, {ent(e), sh::Vec3f{10, 20, 30}});
  // The loop's origin lands at the single-player offset (10), not the multiplayer one (11).
  CHECK(fx.f(e, kSp.f.origin) == 10 && fx.f(e, kSp.f.origin, 1) == 20);
  CHECK(fx.f(e, kSp.f.origin, 2) == 30);
  fx.in_call([&] { fx.set_f(e, kSp.f.movetype, 4); });
  CHECK(fx.s->sv.edicts[static_cast<size_t>(e)].movetype == 4);
  CHECK(fx.str_field(0, kSp.f.message) == "a single-player map");
}

TEST(prints_take_no_level) {
  SoloFixture so;
  Fixture& fx = so.fx;
  const int p = fx.join(0, "player");
  fx.call(23, {"two ", "words"});
  const auto prints = fx.events(sv::EventKind::print);
  CHECK(prints.size() == 1 && prints[0]->text == "two words" && prints[0]->level == 2);
  fx.call(24, {ent(p), "to one ", "client"});
  const auto both = fx.events(sv::EventKind::print);
  CHECK(both.size() == 2 && both[1]->text == "to one client" && both[1]->to == 1u);
}

TEST(aim_turns_onto_a_target_near_the_line_of_sight) {
  SoloFixture so;
  Fixture& fx = so.fx;
  const int shooter = so.box({-416, -256, 24}, false);
  fx.call(1, {sh::Vec3f{0, 0, 0}});  // makevectors: v_forward is +x
  // Nothing to aim at: straight ahead.
  fx.call(44, {ent(shooter), 1000});
  CHECK(fx.rv.x == 1 && fx.rv.y == 0 && fx.rv.z == 0);
  // A target 200 ahead and 60 up: the pitch is corrected, the heading kept.
  const int target = so.box({-216, -256, 84}, true);
  fx.call(44, {ent(shooter), 1000});
  const float dx = 200, dz = 60 + 4 - 20;  // the box's centre, from the line's start
  const float len = std::sqrt(dx * dx + dz * dz);
  CHECK_MSG(near(fx.rv.x, dx / len) && fx.rv.y == 0 && near(fx.rv.z, dz / len),
            std::to_string(fx.rv.x) + " " + std::to_string(fx.rv.z));
  // Off: sv_aim 1.
  sv::server_command(*fx.s, "sv_aim 1");
  fx.call(44, {ent(shooter), 1000});
  CHECK(fx.rv.x == 1 && fx.rv.z == 0);
  // Not aimed damage: straight ahead.
  sv::server_command(*fx.s, "sv_aim 0.93");
  fx.in_call([&] { fx.set_f(target, kSp.f.takedamage, 1); });
  fx.call(44, {ent(shooter), 1000});
  CHECK(fx.rv.x == 1 && fx.rv.z == 0);
}

TEST(no_multicast_and_no_multiplayer_builtins) {
  SoloFixture so;
  Fixture& fx = so.fx;
  fx.call(52, {4, 1});  // WriteByte to destination 4
  CHECK(fx.fault && fx.fault_message.find("(0..3)") != std::string::npos);
  fx.call(52, {2, 1});
  CHECK(!fx.fault && fx.s->out.all.size() == 1);
  for (int n = 79; n <= 82; n++) CHECK(fx.vm().builtins[static_cast<size_t>(n)].fn == nullptr);
  CHECK(fx.vm().builtins[48].fn != nullptr);  // particle
}
