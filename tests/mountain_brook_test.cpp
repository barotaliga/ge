// plugins/mountain/tarn/brook.hpp: a real face's mapped streams are laid on its map. Each runs
// down its face's mapped line, its surface falling and never climbing, under the ground at every
// point; where the generator's ground stands too far over it the line breaks. Open, the bed is cut
// under the surface across the channel and the banks stand just over it, graded to the ground no
// steeper than the slope; frozen, the stream is a shallow gully. No tree stands in a channel. A
// dealt map has none.
//
// ge:summary Test: a real face's mapped streams laid on its map: the falling line, the bed, the ice
// ge:keywords test mapped-stream lay_mapped_streams mapped_streams lay_stream stream_ice brook
// ge:skill mountain testing
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/real_hints.hpp"
#include "mountain/tarn/brook.hpp"
#include "mountain/tarn/mapped.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace tn = ge::mountain::tarn;

const mg::Level& alpine1() {
  static const std::unique_ptr<mg::Level> built = [] {
    mg::GenerateOptions opts;
    opts.face = "alpine-1";
    return std::make_unique<mg::Level>(mg::generate_level(38, opts));
  }();
  return *built;
}

double ground(const mg::Level& level, double x, double z) {
  return ge::core::sample_field(level.ground, x, z);
}

// The map with its lakes and its longest stream laid, on day `day` of the year.
mg::Level with_longest_stream(double day) {
  mg::Level level = alpine1();
  level.sun.day_of_year = day;
  tn::lay_mapped_lakes(level);
  std::vector<mg::MapStream> streams = tn::mapped_streams(level);
  const auto it = std::max_element(
      streams.begin(), streams.end(),
      [](const auto& a, const auto& b) { return a.points.size() < b.points.size(); });
  if (it != streams.end()) tn::lay_stream(level, *it);
  return level;
}

// The plan distance from (x, z) to a stream's line, m, and the surface and width there.
struct Near {
  double d = 1e30, level = 0, width = 0;
};
Near near_of(const mg::MapStream& s, double x, double z) {
  Near best;
  for (size_t i = 0; i + 1 < s.points.size(); i++) {
    const mg::StreamPoint& a = s.points[i];
    const mg::StreamPoint& b = s.points[i + 1];
    const double ex = b.x - a.x, ez = b.z - a.z;
    const double t = std::clamp(((x - a.x) * ex + (z - a.z) * ez) / (ex * ex + ez * ez), 0.0, 1.0);
    const double d = std::hypot(x - a.x - t * ex, z - a.z - t * ez);
    if (d < best.d)
      best = {d, a.level + t * (b.level - a.level), a.width + t * (b.width - a.width)};
  }
  return best;
}

}  // namespace

TEST(a_real_face_s_streams_fall_down_its_mapped_lines) {
  mg::Level level = alpine1();
  tn::lay_mapped_lakes(level);
  const std::vector<mg::MapStream> streams = tn::mapped_streams(level);
  const auto hints = mg::real_hints("alpine-1");
  CHECK(hints.has_value() && !hints->streams.empty());
  CHECK_MSG(!streams.empty(), "alpine-1's map holds no mapped stream: bake its water (real-hints)");
  for (const mg::MapStream& s : streams) {
    CHECK(static_cast<double>(s.points.size()) * tn::kStreamStep >= tn::kShortestRun);
    CHECK(!s.frozen && s.ice == 0);
    const double sink = s.river ? tn::kRiverSink : tn::kBrookSink;
    for (size_t i = 0; i < s.points.size(); i++) {
      const mg::StreamPoint& p = s.points[i];
      if (i > 0) CHECK_MSG(p.level <= s.points[i - 1].level, "a stream's surface climbs");
      const double y = ground(level, p.x, p.z);
      CHECK_MSG(p.level <= y - sink + 1e-9 && y - sink - p.level <= tn::kDeepestCut + 1e-9,
                "a stream's surface stands off its ground: " + std::to_string(y - p.level));
      CHECK(p.width == (s.river ? tn::kRiverWidth : tn::kBrookWidth));
    }
  }
  // A dealt map has no face and so no mapped stream.
  mg::GenerateOptions dealt;
  dealt.version = 10;
  CHECK(tn::mapped_streams(mg::generate_level(38, dealt)).empty());
}

TEST(open_water_runs_in_a_bed_cut_under_its_banks) {
  const mg::Level level = with_longest_stream(200);  // midsummer
  CHECK(level.streams.size() == 1);
  if (level.streams.empty()) return;
  const mg::MapStream& s = level.streams[0];
  CHECK(!s.frozen);
  int wet = 0, dry = 0, steep = 0;
  for (const mg::StreamPoint& p : s.points) {
    // Mid-stream lies under the surface; across from it, the bank grades no steeper than allowed.
    if (ground(level, p.x, p.z) < p.level - 0.25 * p.depth)
      ++wet;
    else
      ++dry;
  }
  const ge::core::Heightfield& g = level.ground;
  const mg::StreamPoint& mid = s.points[s.points.size() / 2];
  for (int row = 0; row < g.rows; ++row) {
    const double z = g.origin_z + row * g.cell;
    if (std::fabs(z - mid.z) > 40) continue;
    for (int col = 0; col < g.cols; ++col) {
      const double x = g.origin_x + col * g.cell;
      if (std::fabs(x - mid.x) > 40) continue;
      const Near n = near_of(s, x, z);
      const double off = n.d - 0.5 * n.width - 0.5 * g.cell;
      if (off < 0 || off > tn::kStreamReach) continue;
      const double y =
          g.data[static_cast<size_t>(row) * static_cast<size_t>(g.cols) + static_cast<size_t>(col)];
      if (std::fabs(y - (n.level + tn::kRim)) > tn::kStreamBank * off + 0.05) ++steep;
    }
  }
  CHECK_MSG(dry * 10 <= wet, std::to_string(dry) + " of its points dry mid-stream");
  CHECK_MSG(steep == 0, std::to_string(steep) + " bank nodes steeper than the slope");
  for (const mg::TreeDef& t : level.trees) {
    const Near n = near_of(s, t.x, t.z);
    CHECK_MSG(n.d >= 0.5 * n.width + tn::kFell - level.cell, "a tree stands in the stream");
  }
}

TEST(a_frozen_stream_is_a_shallow_gully_under_the_snow) {
  const mg::Level level = with_longest_stream(40);  // midwinter
  CHECK(level.streams.size() == 1);
  if (level.streams.empty()) return;
  const mg::MapStream& s = level.streams[0];
  CHECK_MSG(s.frozen && s.ice > 0, "alpine-1's longest stream is open in midwinter");
  int wet = 0;
  for (const mg::StreamPoint& p : s.points) {
    // The gully is shallow: it never dips to where the water would run.
    if (ground(level, p.x, p.z) < p.level) ++wet;
  }
  CHECK_MSG(wet == 0, std::to_string(wet) + " points of the frozen gully down at the water");
}
