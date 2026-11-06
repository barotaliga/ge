// THE CLASSIC MOVE'S OWN BITS AND ITS RULES: every recorded scenario's run through `player_move`
// hashed line by line (FNV-1a 64 over each line's origin and velocity bits and its discrete
// fields) against tests/golden/shooter_pmove.txt, which this test writes itself under
// GE_GOLDEN_WRITE=1 (a deliberate change to the move rewrites it, and the PR says why); the
// same runs twice must agree; and the rules no recording shows: the command split, the move
// types that stand still, the dead player's held jump, the landing fix.
//
// Golden rows: scenario index, line count, digest (hex words).
//
// ge:summary Tests: the classic player move's own bits per scenario, the split and still types
// ge:keywords test pmove golden digest determinism run_command split none lock dead landing
// ge:keywords speedjump easy-jump
// ge:skill shooter testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "golden.hpp"
#include "shooter/pmove.hpp"
#include "shooter/recording.hpp"

namespace sh = ge::shooter;

namespace {

constexpr const char* kGolden = "tests/golden/shooter_pmove.txt";

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
  void add_float(float v) { add(&v, sizeof v); }
};

uint64_t digest_of(const sh::ReplayResult& r) {
  Fnv f;
  for (const sh::LineCheck& c : r.lines) {
    const sh::RecordedState& o = c.ours;
    for (int i = 0; i < 3; i++) f.add_float(o.origin[i]);
    for (int i = 0; i < 3; i++) f.add_float(o.velocity[i]);
    f.add_int(o.groundent);
    f.add_int(o.waterlevel);
    f.add_int(o.watertype);
    f.add_int(o.jump_held ? 1 : 0);
    f.add_float(o.waterjump_time);
    f.add_int(o.blocked);
    for (int e : o.touches) f.add_int(e);
    f.add_int(-99);
  }
  return f.h;
}

// The flat world of the first scene, with no other solid.
struct Flat {
  sh::SceneWorld sw;
  Flat() { sh::build_scene_world(recording().scenes[0], {}, sw); }
};

sh::MoveState standing() {
  sh::MoveState s;
  s.origin = {0, 0, 24};
  return s;
}

bool same_bits(sh::Vec3f a, sh::Vec3f b) {
  for (int i = 0; i < 3; i++)
    if (ge::test::bits(a[i]) != ge::test::bits(b[i])) return false;
  return true;
}

bool same_bits(const sh::MoveState& a, const sh::MoveState& b) {
  return same_bits(a.origin, b.origin) && same_bits(a.velocity, b.velocity) &&
         a.ground == b.ground && a.jump_held == b.jump_held;
}

}  // namespace

TEST(every_scenario_matches_its_own_bits) {
  const sh::Recording& rec = recording();
  std::vector<std::vector<uint64_t>> got;
  for (size_t i = 0; i < rec.scenarios.size(); i++) {
    const sh::ReplayResult a = sh::replay(rec, rec.scenarios[i]);
    const sh::ReplayResult b = sh::replay(rec, rec.scenarios[i]);
    CHECK_MSG(digest_of(a) == digest_of(b),
              rec.scenarios[i].name +
                  " differs between two runs: the move reads state from "
                  "outside its arguments (load the determinism skill)");
    got.push_back({i, a.lines.size(), digest_of(a)});
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# plugins/shooter/pmove.hpp's own record: scenario, lines, digest of every line "
           "(tests/shooter_pmove_test.cpp)\n";
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
                                           "GE_GOLDEN_WRITE=1 if the move changed on purpose");
  for (size_t i = 0; i < rows.size() && i < got.size(); i++) {
    CHECK_MSG(rows[i] == got[i], rec.scenarios[i].name +
                                     " moved off its golden bits: if the change is meant, "
                                     "check tests/shooter_recording_test.cpp still passes, then "
                                     "rewrite with GE_GOLDEN_WRITE=1 and say why in the PR");
  }
}

TEST(a_long_command_runs_as_its_halves) {
  Flat flat;
  sh::MoveSettings set;
  sh::MoveCmd cmd;
  cmd.forward = 400;
  cmd.angles = {0, 30, 0};

  // 75 ms: two halves of 37 (the odd millisecond is lost).
  sh::MoveState a = standing(), b = standing();
  cmd.msec = 75;
  sh::run_command(a, cmd, set, flat.sw.world);
  cmd.msec = 37;
  sh::player_move(b, cmd, set, flat.sw.world);
  sh::player_move(b, cmd, set, flat.sw.world);
  CHECK(same_bits(a, b));

  // 200 ms: halves of 100, each split again into 50 + 50.
  a = standing();
  b = standing();
  cmd.msec = 200;
  sh::run_command(a, cmd, set, flat.sw.world);
  cmd.msec = 50;
  for (int i = 0; i < 4; i++) sh::player_move(b, cmd, set, flat.sw.world);
  CHECK(same_bits(a, b));

  // 50 ms or less runs whole.
  a = standing();
  b = standing();
  cmd.msec = 50;
  sh::run_command(a, cmd, set, flat.sw.world);
  sh::player_move(b, cmd, set, flat.sw.world);
  CHECK(same_bits(a, b));
}

TEST(none_and_lock_stand_still_and_are_categorised) {
  Flat flat;
  sh::MoveSettings set;
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.forward = 400;
  cmd.angles = {10, 20, 0};
  for (const sh::MoveType t : {sh::MoveType::none, sh::MoveType::lock}) {
    sh::MoveState s = standing();
    s.type = t;
    s.velocity = {100, 0, 0};
    s.watertype = 0;
    sh::player_move(s, cmd, set, flat.sw.world);
    CHECK(s.origin == (sh::Vec3f{0, 0, 24}));
    CHECK(s.velocity == (sh::Vec3f{100, 0, 0}));
    CHECK(s.ground == 0);                      // categorised: on the world
    CHECK(s.watertype == sh::kContentsEmpty);  // categorised: dry
    CHECK(s.angles == (sh::Vec3f{0, 0, 0}));   // the view is not taken
  }
}

TEST(the_dead_hold_the_jump_and_do_not_accelerate) {
  Flat flat;
  sh::MoveSettings set;
  sh::MoveState s = standing();
  s.type = sh::MoveType::dead;
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.forward = 400;
  sh::player_move(s, cmd, set, flat.sw.world);
  CHECK(s.jump_held);
  CHECK(s.velocity == (sh::Vec3f{0, 0, 0}));
  CHECK(s.origin == (sh::Vec3f{0, 0, 24}));
}

TEST(a_jump_needs_the_button_let_go) {
  Flat flat;
  sh::MoveSettings set;
  sh::MoveState s = standing();
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.buttons = sh::kButtonJump;
  sh::player_move(s, cmd, set, flat.sw.world);
  CHECK(s.ground == -1 && s.jump_held);
  CHECK(s.velocity.z > 250);
  // Held through the landing: no second jump.
  for (int i = 0; i < 100 && s.ground == -1; i++) sh::player_move(s, cmd, set, flat.sw.world);
  CHECK(s.ground == 0);
  sh::player_move(s, cmd, set, flat.sw.world);
  CHECK(s.ground == 0);
  // Let go, pressed again: it jumps.
  cmd.buttons = 0;
  sh::player_move(s, cmd, set, flat.sw.world);
  cmd.buttons = sh::kButtonJump;
  sh::player_move(s, cmd, set, flat.sw.world);
  CHECK(s.ground == -1);
}

TEST(a_spectator_flies_through_the_floor) {
  Flat flat;
  sh::MoveSettings set;
  sh::MoveState s = standing();
  s.type = sh::MoveType::spectator;
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.angles = {90, 0, 0};  // looking straight down
  cmd.forward = 400;
  for (int i = 0; i < 60; i++) sh::player_move(s, cmd, set, flat.sw.world);
  CHECK(s.origin.z < -24);
  CHECK(s.ground == -1);
  CHECK(sh::length(s.velocity) <= set.spectatormaxspeed + 0.01f);
}

// THE EASY JUMP (pm_speedjump): forward and jump held hop on and gain; a strafe gains more; off,
// or with a side key, it is the classic move to the bit.
namespace {

constexpr int kSecond = 77;  // commands of 13 ms

// The heading of the plan velocity, degrees counter-clockwise from +x.
float heading_of(sh::Vec3f v) {
  return static_cast<float>(ge::core::libm::atan2(v.y, v.x) * 180 / 3.14159265358979323846);
}

float plan_speed(sh::Vec3f v) { return sh::root_of(v.x * v.x + v.y * v.y); }

// A second of running east on the ground, to sv_maxspeed.
sh::MoveState run_up(const Flat& flat, const sh::MoveSettings& set) {
  sh::MoveState s = standing();
  s.origin.x = -3000;
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.forward = 400;
  for (int i = 0; i < kSecond; i++) sh::player_move(s, cmd, set, flat.sw.world);
  return s;
}

// Forward and jump held for `seconds`, the view on `yaw`; the jumps counted.
int forward_jump(const Flat& flat, const sh::MoveSettings& set, sh::MoveState& s, int seconds,
                 float yaw = 0) {
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.forward = 400;
  cmd.buttons = sh::kButtonJump;
  cmd.angles = {0, yaw, 0};
  int jumps = 0;
  for (int i = 0; i < seconds * kSecond; i++) {
    const bool was_down = s.ground != -1;
    sh::player_move(s, cmd, set, flat.sw.world);
    if (was_down && s.velocity.z > 200) jumps++;
  }
  return jumps;
}

// The best strafe: the side key alone (swapped each hop, held through the takeoff too), the view
// on the way he moves, jump pressed on the command after landing and let go in the air.
void best_strafe(const Flat& flat, const sh::MoveSettings& set, sh::MoveState& s, int seconds) {
  int side = 400;
  for (int i = 0; i < seconds * kSecond; i++) {
    sh::MoveCmd cmd;
    cmd.msec = 13;
    const bool down = s.ground != -1;
    if (down) side = -side;
    cmd.side = static_cast<int16_t>(side);
    cmd.buttons = down ? sh::kButtonJump : 0;
    cmd.angles = {0, heading_of(s.velocity), 0};
    sh::player_move(s, cmd, set, flat.sw.world);
  }
}

}  // namespace

TEST(the_easy_jump_off_leaves_forward_and_jump_classic) {
  Flat flat;
  sh::MoveSettings set;  // pm_speedjump 0
  sh::MoveState s = run_up(flat, set);
  const int jumps = forward_jump(flat, set, s, 3);
  CHECK(jumps == 1);  // a held jump does not hop again
  CHECK(plan_speed(s.velocity) <= 320.01f);
}

TEST(the_easy_jump_hops_on_and_gains_holding_forward_and_jump) {
  Flat flat;
  sh::MoveSettings set;
  set.speedjump = 0.5f;
  sh::MoveState s = run_up(flat, set);
  const int jumps = forward_jump(flat, set, s, 5);
  CHECK(jumps >= 6);  // a jump a landing: about 0.7 s each
  CHECK_MSG(plan_speed(s.velocity) > 450, "5 s of the easy jump should pass 450 units/s");
  CHECK(sh::length(s.velocity) < 2000);
  // Straight on: the view never left +x.
  CHECK(s.velocity.y > -1 && s.velocity.y < 1);
}

TEST(a_best_strafe_still_gains_more_than_the_easy_jump) {
  Flat flat;
  sh::MoveSettings set;
  set.speedjump = 0.5f;
  sh::MoveState easy = run_up(flat, set), strafe = easy;
  forward_jump(flat, set, easy, 5);
  best_strafe(flat, set, strafe, 5);
  CHECK_MSG(plan_speed(strafe.velocity) > plan_speed(easy.velocity) + 50,
            "the best strafe must out-gain the easy jump");
}

TEST(a_strafe_moves_to_the_bit_with_the_easy_jump_allowed) {
  Flat flat;
  sh::MoveSettings off, on;
  on.speedjump = 0.5f;
  sh::MoveState a = run_up(flat, off), b = run_up(flat, on);
  CHECK(same_bits(a, b));
  best_strafe(flat, off, a, 4);
  best_strafe(flat, on, b, 4);
  CHECK(same_bits(a, b));
}

TEST(the_easy_jump_turns_toward_the_view) {
  Flat flat;
  sh::MoveSettings set;
  set.speedjump = 0.5f;
  sh::MoveState s = run_up(flat, set);
  s.origin.x = 0;
  forward_jump(flat, set, s, 2, 30);  // the view 30 degrees left of the way
  const float h = heading_of(s.velocity);
  CHECK_MSG(h > 29 && h < 31, "the way should have turned to the view's 30 degrees");
}
