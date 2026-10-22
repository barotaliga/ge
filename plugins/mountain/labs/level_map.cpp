// The map lab helpers: the build under the lab's flags, and the painter.
//
// ge:summary The map labs' build-from-flags and the from-above painter
// ge:keywords lab level map draw hillshade build-map
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include "mountain/labs/level_map.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "mountain/mapgen/generate.hpp"

namespace ge::mountain::labs {

using ge::lab::Rgba;

namespace {

uint8_t mix(double a, double b, double t) { return static_cast<uint8_t>(a + (b - a) * t); }

}  // namespace

std::optional<mapgen::RegionId> region_named(const std::string& name) {
  if (name == "alpine") return mapgen::RegionId::alpine;
  if (name == "fell") return mapgen::RegionId::fell;
  if (name == "continental") return mapgen::RegionId::continental;
  if (name == "maritime") return mapgen::RegionId::maritime;
  return std::nullopt;
}

std::vector<int> map_versions(const ge::lab::Lab& lab) {
  std::vector<int> out;
  for (const double v : lab.arg_nums("version")) out.push_back(static_cast<int>(v));
  return out;
}

std::string map_id(double seed, int version) {
  const std::string id = "seed-" + std::to_string(static_cast<long long>(seed));
  return version == 1 ? id : id + "-v" + std::to_string(version);
}

std::optional<mapgen::Level> build_map(ge::lab::Lab& lab, double seed, std::optional<int> version,
                                       std::optional<mapgen::PisteGrade> grade,
                                       std::optional<std::string> course, bool tricks) {
  mapgen::GenerateOptions opts;
  opts.tricks = tricks;
  const int v = version ? *version : map_versions(lab).front();
  opts.version = v;
  if (v != 1) {
    opts.grade = grade;
    opts.course = std::move(course);
  }
  opts.region = region_named(lab.arg("region"));
  if (!opts.region) {
    lab.fail("--region=" + lab.arg("region") + " is not a region",
             "next: rerun with alpine, fell, continental or maritime (mapgen/regions.hpp).");
    return std::nullopt;
  }
  try {
    return mapgen::generate_level(seed, opts);
  } catch (const mapgen::GenerateError& e) {
    lab.fail("seed " + std::to_string(static_cast<long long>(seed)) + " did not build: " + e.what(),
             "Run `make lab LAB=analyze ARGS=\"--seeds=" +
                 std::to_string(static_cast<long long>(seed)) + " --version=" + std::to_string(v) +
                 " --refusals\"` to see each attempt's refusal. Skill: mountain.");
    return std::nullopt;
  }
}

std::vector<std::pair<std::string, std::string>> map_inputs(const ge::lab::Lab& lab,
                                                            const mapgen::Level& level) {
  return {{"seed", std::to_string(static_cast<long long>(level.seed))},
          {"version", std::to_string(level.version)},
          {"region", lab.arg("region")},
          {"attempt", std::to_string(level.attempt)}};
}

ge::lab::Picture draw_map(const mapgen::Level& level, int px) {
  const double k = px / level.size;  // pixels a metre
  auto pic = ge::lab::create_picture(px, px, {0, 0, 0});
  const ge::core::Heightfield& g = level.ground;

  // The face: height to colour (the valley's grey-green to the summit's white), shaded by the
  // slope's facing to a light from the upper left, and the packed snow tinted blue.
  double lo = g.data.empty() ? 0 : g.data[0], hi = lo;
  for (const float h : g.data) {
    lo = std::min(lo, static_cast<double>(h));
    hi = std::max(hi, static_cast<double>(h));
  }
  const double span = hi > lo ? hi - lo : 1;
  for (int py = 0; py < px; py++) {
    for (int pxx = 0; pxx < px; pxx++) {
      const double x = (pxx + 0.5) / k, z = (py + 0.5) / k;
      const double t = (level.ground_at(x, z) - lo) / span;
      const auto s = ge::core::field_gradient(g, x, z);
      // Light from -x, -z, 45 degrees up: the normal (-gx, 1, -gz) against (-1, 1.41, -1)/2.
      const double nl = std::sqrt(s.gx * s.gx + 1 + s.gz * s.gz);
      const double lit = std::clamp((s.gx + 1.41421356 + s.gz) / (2 * nl), 0.0, 1.0);
      const double shade = 0.45 + 0.55 * lit;
      const double packed = std::clamp(level.packed_at(x, z), 0.0, 1.0);
      const double r = mix(mix(118, 250, t), 150, packed * 0.5) * shade;
      const double gg = mix(mix(130, 250, t), 190, packed * 0.5) * shade;
      const double b = mix(mix(120, 255, t), 255, packed * 0.5) * shade;
      ge::lab::set_pixel(
          pic, pxx, py,
          {static_cast<uint8_t>(r), static_cast<uint8_t>(gg), static_cast<uint8_t>(b)});
    }
  }

  for (const mapgen::TreeDef& tree : level.trees) {
    ge::lab::fill_disc(pic, tree.x * k, tree.z * k, std::max(0.7, tree.crown * k),
                       {28, 72, 40, 220});
  }

  const auto& pts = level.track.points;
  for (size_t i = 1; i < pts.size(); i++) {
    ge::lab::draw_line(pic, pts[i - 1].x * k, pts[i - 1].z * k, pts[i].x * k, pts[i].z * k,
                       {200, 40, 40}, px >= 768 ? 2 : 1);
  }

  for (const mapgen::Cliff& c : level.cliffs) {
    const double dx = ge::core::libm::cos(c.heading), dz = -ge::core::libm::sin(c.heading);
    const double h = c.width / 2;
    ge::lab::draw_line(pic, (c.x - dx * h) * k, (c.z - dz * h) * k, (c.x + dx * h) * k,
                       (c.z + dz * h) * k, {120, 40, 150}, 2);
    ge::lab::draw_text(pic, c.x * k + 4, c.z * k - 4, c.id, {120, 40, 150});
  }
  for (const mapgen::Kicker& kk : level.kickers) {
    ge::lab::fill_disc(pic, kk.x * k, kk.z * k, 3, {240, 140, 20});
    ge::lab::draw_text(pic, kk.x * k + 5, kk.z * k - 3, kk.id, {150, 70, 0});
  }

  // Gates: a bar across the piste at its width (heading 0 = +z, clockwise from above, so the
  // way across is (cos h, -sin h)), numbered; the start gate green, the finish black.
  for (size_t i = 0; i < level.checkpoints.size(); i++) {
    const mapgen::Checkpoint& cp = level.checkpoints[i];
    const double ax = ge::core::libm::cos(cp.heading), az = -ge::core::libm::sin(cp.heading);
    const double h = cp.width / 2;
    const bool first = i == 0, last = i + 1 == level.checkpoints.size();
    const Rgba c =
        first  ? Rgba{30, 160, 60}
        : last ? Rgba{0, 0, 0}
               : (cp.colour == mapgen::GateColour::red ? Rgba{220, 30, 30} : Rgba{30, 70, 220});
    ge::lab::draw_line(pic, (cp.x - ax * h) * k, (cp.z - az * h) * k, (cp.x + ax * h) * k,
                       (cp.z + az * h) * k, c, 2);
    const std::string label = first ? "START" : last ? "FINISH" : std::to_string(i);
    ge::lab::draw_text(pic, (cp.x + ax * h) * k + 3, cp.z * k - 3, label, c);
  }
  ge::lab::fill_disc(pic, level.spawn.x * k, level.spawn.z * k, 3, {30, 160, 60});
  return pic;
}

}  // namespace ge::mountain::labs
