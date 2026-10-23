// The scripted subjects (engine/camera/scenarios.hpp) and every reference rig through them: a
// scenario sampled at 30 Hz and at 240 Hz follows the same path, its moments arrive once each,
// and every rig of the reference ladder through every scenario at 60 Hz is a pure function of
// the script, its poses' bits held by tests/golden/camera_scenarios.txt.
//
// A deliberate change to a rig or a script moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/camera_scenarios_test`, and put the camera labs' tables
// before and after in the PR (skill: camera).
//
// ge:summary Tests: the scripted subjects' rate independence and every rig's golden digest
// ge:keywords test camera scenarios golden digest determinism rate-independence moments
// ge:skill camera testing
// ge:test tests/camera_scenarios_test.cpp
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "camera/camera.hpp"
#include "camera/ladders.hpp"
#include "camera/scenarios.hpp"
#include "check.hpp"
#include "golden.hpp"

namespace cam = ge::camera;

namespace {

constexpr const char* kGolden = "tests/golden/camera_scenarios.txt";

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

std::string hex(uint64_t u) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u));
  return b;
}

void mix(uint64_t& h, double v) {
  uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  for (int i = 0; i < 8; ++i) {
    h ^= (u >> (8 * i)) & 0xff;
    h *= 0x100000001b3ull;
  }
}

// The digest of one rig of the reference ladder through one scenario at 60 Hz.
uint64_t digest(const cam::Scenario& sc, int rung) {
  const cam::World w = cam::scenario_world(sc);
  cam::ScriptRun r = cam::start_script(sc);
  const auto l = cam::reference_ladder();
  cam::Camera c = cam::make_camera(l.data(), static_cast<int>(l.size()), 1);
  c.rung = rung;
  uint64_t h = 0xcbf29ce484222325ull;
  for (int i = 1; i <= static_cast<int>(sc.seconds * 60); ++i) {
    const cam::Subject s = cam::advance_to(r, i / 60.0);
    const cam::Pose p = cam::update(c, s, w, 1 / 60.0);
    for (double v : {p.eye.x, p.eye.y, p.eye.z, p.yaw, p.pitch, p.roll, p.fov_y}) mix(h, v);
  }
  return h;
}

}  // namespace

TEST(a_scenario_follows_the_same_path_at_any_rate) {
  for (const cam::Scenario& sc : cam::camera_scenarios()) {
    cam::ScriptRun a = cam::start_script(sc), b = cam::start_script(sc);
    int moments_a = 0, moments_b = 0;
    for (int i = 1; i <= static_cast<int>(sc.seconds * 240); ++i) {
      const cam::Subject sb = cam::advance_to(b, i / 240.0);
      moments_b += sb.event_count;
      if (i % 8 != 0) continue;
      const cam::Subject sa = cam::advance_to(a, i / 240.0);
      moments_a += sa.event_count;
      CHECK_MSG(std::fabs(sa.pos.x - sb.pos.x) < 1e-9 && std::fabs(sa.pos.z - sb.pos.z) < 1e-9 &&
                    std::fabs(sa.pos.y - sb.pos.y) < 1e-9,
                std::string(sc.id) + " moved differently sampled at 30 and 240 Hz");
    }
    CHECK_MSG(moments_a == moments_b, std::string(sc.id) + ": the moments arrived " +
                                          std::to_string(moments_a) + " times at 30 Hz and " +
                                          std::to_string(moments_b) + " at 240 Hz");
  }
}

TEST(every_rig_through_every_scenario_is_pure_and_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto l = cam::reference_ladder();
  std::string got;
  for (const cam::Scenario& sc : cam::camera_scenarios()) {
    std::string line(sc.id);
    for (int i = 0; i < static_cast<int>(l.size()); ++i) {
      const uint64_t a = digest(sc, i);
      CHECK_MSG(a == digest(sc, i), std::string(sc.id) + " through " + std::string(l[i].name) +
                                        " is not a pure function of its script");
      line += " " + hex(a);
    }
    got += line + "\n";
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# scenario then a digest a rig of the reference ladder (eyes chase far high helmet "
           "tips orbit trackside), 60 Hz (camera_scenarios_test)\n"
        << got;
    return;
  }
  std::string want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want += line + "\n";
  }
  CHECK_MSG(got == want, "the camera's rigs moved:\n" + got + "want\n" + want +
                             "If the move is meant, rewrite " + kGolden +
                             " with GE_GOLDEN_WRITE=1 and put the camera labs before and after "
                             "in the PR (skill: camera).");
}
