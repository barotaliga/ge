// THE HULL TRACE HELD TO ITS RECORDINGS: every single trace of tests/golden/shooter_hulltrace.txt
// (corners where a slope meets a wall, slants, starts on a plane, starts in solid that leave it,
// all-solid starts) run through `trace_hull_original` (the original walk) over its scene's hull 0
// or hull 1, built as the classic move's recordings build them, each within 1e-4 on the fraction
// and the plane and the move's position tolerance on the end point, with every flag and the
// step-back count equal.
//
// ge:summary Tests: the hull trace against every recorded single trace, corners and slants
// ge:keywords test hull trace hulltrace recording vectors corner slant dist-epsilon backup
// ge:skill shooter testing
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooter/recording.hpp"

namespace sh = ge::shooter;

namespace {

std::string read_file(const char* path) {
  std::ifstream f(ge::test::root_path(path));
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

float diff(sh::Vec3f a, sh::Vec3f b) {
  float m = 0;
  for (int i = 0; i < 3; i++) m = std::fmax(m, std::fabs(a[i] - b[i]));
  return m;
}

}  // namespace

TEST(every_recorded_trace_matches) {
  sh::Recording rec;
  std::string error;
  CHECK_MSG(sh::parse_recording(read_file("tests/golden/shooter_recording.txt"), rec, error),
            error);
  const std::string text = read_file("tests/golden/shooter_hulltrace.txt");
  std::istringstream lines(text);
  std::string line, scene;
  int traces = 0, passed = 0, line_no = 0;
  while (std::getline(lines, line)) {
    line_no++;
    const std::vector<std::string_view> t = sh::split_words(line);
    if (t.empty() || t[0][0] == '#') continue;
    if (t[0] == "scene") {
      scene = std::string(t[1]);
      continue;
    }
    CHECK_MSG(t[0] == "trace" && t.size() == 36, "line " + std::to_string(line_no));
    if (t.size() != 36) continue;
    auto num = [&](size_t i) {
      float v = 0;
      CHECK(sh::parse_float(t[i], v));
      return v;
    };
    auto vec = [&](size_t i) { return sh::Vec3f{num(i), num(i + 1), num(i + 2)}; };
    const int hull = static_cast<int>(num(2));
    const sh::Vec3f start = vec(4), end = vec(8);
    const float want_fraction = num(13);
    const sh::Vec3f want_end = vec(15), want_normal = vec(19);
    const float want_dist = num(23);
    const bool want[6] = {num(25) != 0, num(27) != 0, num(29) != 0,
                          num(31) != 0, num(33) != 0, false};
    const int want_backups = static_cast<int>(num(35));

    const sh::RecordedScene* sc = nullptr;
    for (const sh::RecordedScene& s : rec.scenes)
      if (s.name == scene) sc = &s;
    CHECK_MSG(sc != nullptr, "no scene " + scene);
    if (sc == nullptr) continue;
    sh::SceneWorld w;
    sh::build_scene_world(*sc, {}, w);
    const sh::HullTrace tr =
        sh::trace_hull_original(hull == 0 ? w.point_hull.hull() : w.clip_hull.hull(), start, end);
    const bool hit = tr.fraction < 1 || tr.startsolid;
    const bool ok = std::fabs(tr.fraction - want_fraction) <= 1e-4f &&
                    diff(tr.endpos, want_end) <= sh::kPositionTolerance &&
                    (!hit || tr.fraction == 1 ||
                     (diff(tr.plane.normal, want_normal) <= 1e-4f &&
                      std::fabs(tr.plane.dist - want_dist) <= 1e-3f)) &&
                    tr.allsolid == want[0] && tr.startsolid == want[1] && tr.inopen == want[2] &&
                    tr.inwater == want[3] && hit == want[4] && tr.backups == want_backups;
    traces++;
    if (ok) {
      passed++;
      continue;
    }
    char buf[300];
    std::snprintf(buf, sizeof buf,
                  "line %d (%s): ours fraction %.9g end %g %g %g normal %g %g %g flags %d%d%d%d "
                  "backups %d; next: read plugins/shooter/hull.cpp's check",
                  line_no, scene.c_str(), tr.fraction, tr.endpos.x, tr.endpos.y, tr.endpos.z,
                  tr.plane.normal.x, tr.plane.normal.y, tr.plane.normal.z, tr.allsolid,
                  tr.startsolid, tr.inopen, tr.inwater, tr.backups);
    CHECK_MSG(false, std::string(buf));
  }
  std::printf("%d of %d recorded traces match\n", passed, traces);
  CHECK(traces == 31 && passed == traces);
}
