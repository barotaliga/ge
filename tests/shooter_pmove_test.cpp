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
// ge:skill shooter testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
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
