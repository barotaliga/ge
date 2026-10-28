// A room level's movers: the vault's trigger opens its door, a walker passes through it, a body
// standing in the doorway keeps the door from closing and it goes back open, and with the doorway
// clear it closes; a plat a body steps onto rises with the body on it; a train runs its corners
// there and back; and two runs of the same steps end with the same bits.
//
// ge:summary Test: doors, triggers, plats and trains moved and the bodies they carry
// ge:keywords test rooms movers door trigger plat train carry rider vault
// ge:skill rooms move testing
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "move/spec.hpp"
#include "move/step.hpp"
#include "rooms/movers.hpp"
#include "rooms/text.hpp"

namespace {

namespace rm = ge::rooms;
namespace mv = ge::move;
constexpr double kDt = 1.0 / 120.0;

rm::Level load(const char* rel) {
  std::ifstream in(ge::test::root_path(rel));
  std::stringstream s;
  s << in.rdbuf();
  rm::Level level;
  std::string error;
  CHECK_MSG(rm::parse(s.str(), &level, &error), error);
  return level;
}

rm::Level parse(const std::string& text) {
  rm::Level level;
  std::string error;
  CHECK_MSG(rm::parse(text, &level, &error), error);
  return level;
}

rm::Movers movers_of(const rm::Level& level) {
  rm::Movers m;
  std::string error;
  CHECK_MSG(rm::build_movers(level, 2, &m, &error), error);
  return m;
}

// One step: the movers, then the body walking `forward` along heading `yaw`.
void step(rm::Movers& m, const mv::MoveSpec& spec, mv::MoveState& s, double forward, double yaw) {
  rm::step_movers(m, spec, std::span<mv::MoveState>(&s, 1), kDt);
  mv::MoveCmd cmd;
  cmd.forward = forward;
  cmd.yaw = yaw;
  mv::step_move(spec, rm::movers_world(m), s, cmd, kDt);
}

// A box brush in the text form, every face `stone`.
std::string box(const char* head, double x0, double y0, double z0, double x1, double y1,
                double z1) {
  char b[512];
  std::snprintf(b, sizeof b,
                "brush %s\n"
                "face 1,0,0 %g stone s=0,32,0,0 t=0,0,32,0\n"
                "face -1,0,0 %g stone s=0,32,0,0 t=0,0,32,0\n"
                "face 0,1,0 %g stone s=32,0,0,0 t=0,0,32,0\n"
                "face 0,-1,0 %g stone s=32,0,0,0 t=0,0,32,0\n"
                "face 0,0,1 %g stone s=32,0,0,0 t=0,-32,0,0\n"
                "face 0,0,-1 %g stone s=32,0,0,0 t=0,-32,0,0\n\n",
                head, x1, 0.0 - x0, y1, 0.0 - y0, z1, 0.0 - z0);
  return b;
}

const char* kHead = "material stone rgb=128,128,128 size=64,64\n\n";

}  // namespace

TEST(the_vaults_trigger_opens_its_door_and_a_body_in_the_doorway_holds_it) {
  const rm::Level level = load("plugins/rooms/levels/vault.gelv.txt");
  rm::Movers m = movers_of(level);
  CHECK(m.movers.size() == 1);
  CHECK(m.triggers.size() == 1);
  CHECK(m.movers[0].named);
  CHECK(std::fabs(m.movers[0].pos2.x + 0.95) < 1e-9);
  const mv::MoveSpec spec = *mv::find_preset("walker");
  mv::MoveState s = mv::stand_at(spec, rm::movers_world(m), {4, 0.3, 3});
  CHECK(s.grounded);
  // Standing back: nothing moves.
  for (int i = 0; i < 120; ++i) step(m, spec, s, 0, 0);
  CHECK(m.movers[0].offset.x == 0);
  // Walking north: the trigger fires, the door slides open, the body walks through the doorway.
  bool fired = false;
  int steps = 0;
  for (; steps < 1200 && s.pos.z < 6.3; ++steps) {
    step(m, spec, s, 1, 0);
    for (const rm::MoverEvent& e : m.events) fired = fired || e.kind == rm::MoverEventKind::fired;
  }
  CHECK(fired);
  CHECK_MSG(s.pos.z >= 6.3, "reached z " + std::to_string(s.pos.z));
  // Standing in the doorway past the door's wait: it tries to close, meets the body, goes back.
  bool closing = false;
  double least = -1e9;  // the most closed the door got
  for (int i = 0; i < 120 * 8; ++i) {
    step(m, spec, s, 0, 0);
    closing = closing || m.movers[0].phase == rm::Phase::coming;
    least = std::max(least, m.movers[0].offset.x);
  }
  CHECK(closing);
  CHECK_MSG(least < -0.5, "the door came back to x " + std::to_string(least) + " body " +
                              std::to_string(s.pos.x) + "," + std::to_string(s.pos.y) + "," +
                              std::to_string(s.pos.z));
  // Back out of the doorway and away: the door closes.
  for (int i = 0; i < 120 * 3; ++i) step(m, spec, s, 1, 3.14159265358979);
  for (int i = 0; i < 120 * 6; ++i) step(m, spec, s, 0, 0);
  CHECK_MSG(m.movers[0].phase == rm::Phase::rest && m.movers[0].offset.x == 0,
            "the door at x " + std::to_string(m.movers[0].offset.x));
}

TEST(a_plat_rises_with_the_body_standing_on_it) {
  const rm::Level level = parse(
      std::string(kHead) + "group lift\n\n" + box("solid", -4, -0.5, -4, 8, 0, 8) +
      box("solid group=lift", 0, 0.9, 0, 2, 1, 2) + "entity func_plat group=lift\nkey height 1\n");
  rm::Movers m = movers_of(level);
  CHECK(m.movers.size() == 1);
  CHECK(std::fabs(m.movers[0].offset.y + 1) < 1e-9);  // resting lowered
  const mv::MoveSpec spec = *mv::find_preset("walker");
  mv::MoveState s = mv::stand_at(spec, rm::movers_world(m), {1, 0.3, 1});
  CHECK(s.grounded);
  for (int i = 0; i < 120; ++i) step(m, spec, s, 0, 0);
  CHECK_MSG(std::fabs(m.movers[0].offset.y) < 1e-9,
            "plat at " + std::to_string(m.movers[0].offset.y));
  CHECK_MSG(s.grounded && std::fabs(s.pos.y - 1) < 0.02, "body at y " + std::to_string(s.pos.y));
}

TEST(a_train_runs_its_corners_there_and_back_to_the_same_bits) {
  const std::string text =
      std::string(kHead) + "group cart\n\n" + box("solid", -4, -0.5, -4, 12, 0, 12) +
      box("solid group=cart", 5, 0, 5, 6, 0.5, 6) +
      "entity func_train group=cart\nkey target c1\nkey speed 3\n\n"
      "entity path_corner\nkey targetname c1\nkey target c2\nkey origin 5 0 5\n\n"
      "entity path_corner\nkey targetname c2\nkey target c1\nkey origin 5 0 8\n";
  const rm::Level level = parse(text);
  const mv::MoveSpec spec = *mv::find_preset("walker");
  const auto run = [&](int steps) {
    rm::Movers m = movers_of(level);
    mv::MoveState s = mv::stand_at(spec, rm::movers_world(m), {0, 0.3, 0});
    double far = 0;
    for (int i = 0; i < steps; ++i) {
      step(m, spec, s, 0, 0);
      far = std::max(far, m.movers[0].offset.z);
    }
    CHECK_MSG(std::fabs(far - 3) < 1e-9, "the train reached z offset " + std::to_string(far));
    return m.movers[0].offset;
  };
  // Out in a second, back in the next: after 1.5 s it is half way home.
  const mv::Vec3 a = run(180);
  CHECK_MSG(std::fabs(a.z - 1.5) < 0.06, "at z offset " + std::to_string(a.z));
  const mv::Vec3 b = run(180);
  CHECK(std::memcmp(&a, &b, sizeof a) == 0);
}
