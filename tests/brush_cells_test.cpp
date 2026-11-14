// THE CELLS AS BRUSHES: the version 29 fixture's point tree given brushes (brush/cells.hpp) stops
// a swept player box where the map's own clip hull for that box does: dropped in each room, thrown
// at a wall and into a corner; the walls are solid and the rooms empty; the same map gives the
// same brushes twice; a map with brushes is left alone.
//
// ge:summary Test: a version 29 map's cells made brushes stop a box where its clip hull does
// ge:keywords test cells brushes cells_as_brushes brush-trace v29 clip-hull fixture parity bevel
// ge:skill brush testing
#include <cstdio>
#include <string>
#include <vector>

#include "brush/cells.hpp"
#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace {

namespace br = ge::brush;

br::Map read(br::Family f) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK(br::write_bsp(br::sample_fixture(), f, &bytes, &error));
  br::Map map;
  const bool ok = br::read_bsp(bytes, &map, &error);
  if (!ok) std::printf("  %s\n", error.c_str());
  CHECK(ok);
  return map;
}

bool near(float a, float b, float eps) { return a - b <= eps && b - a <= eps; }

constexpr float kMins[3] = {-16, -16, -24}, kMaxs[3] = {16, 16, 32};

br::Trace sweep(br::BrushTracer& t, const float a[3], const float b[3]) {
  return t.trace(a, b, kMins, kMaxs, br::kCellSolid);
}

}  // namespace

// Where a swept box stopped, read against the map's own clip hull for the same box (hull 1): the
// end is open there, and a step on along the sweep is not (or the sweep ran its whole way).
bool stops_where_the_hull_does(const br::Map& m, const br::Trace& t, const float from[3],
                               const float to[3]) {
  if (br::hull_contents(m, t.end, 0, 1) != -1) return false;
  if (t.fraction >= 1) return true;
  float dir[3], len2 = 0;
  for (int a = 0; a < 3; ++a) {
    dir[a] = to[a] - from[a];
    len2 += dir[a] * dir[a];
  }
  // A unit step: the sweeps here run along one axis or the diagonal, so scale by the larger part.
  float big = 0;
  for (float d : dir) big = d > big ? d : (-d > big ? -d : big);
  float on[3];
  for (int a = 0; a < 3; ++a) on[a] = t.end[a] + dir[a] / big * 1.0f;
  return len2 > 0 && br::hull_contents(m, on, 0, 1) != -1;
}

TEST(the_29_cells_made_brushes_stop_a_box_where_its_clip_hull_does) {
  br::Map cells = read(br::Family::v29);
  const br::Map hulls = read(br::Family::v29);
  const br::CellCount n = br::cells_as_brushes(cells);
  std::printf("  %u brushes, %u sides, %u bevels, %u planes bound nothing\n", n.brushes, n.sides,
              n.bevels, n.dropped);
  CHECK(n.brushes > 0);
  br::BrushTracer a(cells);
  for (const br::FixtureRoom& r : br::sample_fixture().rooms) {
    const float mid[3] = {float(r.min[0] + r.max[0]) / 2, float(r.min[1] + r.max[1]) / 2,
                          float(r.min[2] + r.max[2]) / 2};
    CHECK(a.contents(mid) == 0);
    const float below[3] = {mid[0], mid[1], float(r.min[2] - 100)};
    const float east[3] = {float(r.max[0] + 400), mid[1], mid[2]};
    const float corner[3] = {float(r.max[0] + 64), float(r.max[1] + 64), mid[2]};
    for (const float* to : {below, east, corner}) {
      const br::Trace t = sweep(a, mid, to);
      CHECK(!t.start_solid);
      const bool ok = stops_where_the_hull_does(hulls, t, mid, to);
      if (!ok)
        std::printf("  %s: the sweep to %.0f %.0f %.0f stopped at %.3f %.3f %.3f\n", r.name.c_str(),
                    double(to[0]), double(to[1]), double(to[2]), double(t.end[0]), double(t.end[1]),
                    double(t.end[2]));
      CHECK(ok);
    }
    const br::Trace drop = sweep(a, mid, below);
    CHECK(near(drop.end[2], float(r.min[2]) + 24, 0.0625f));
    const float wall[3] = {mid[0], mid[1], float(r.min[2] - 8)};
    CHECK(a.contents(wall) & br::kCellSolid);
  }
}

TEST(the_same_map_gives_the_same_brushes_and_a_map_with_brushes_is_left_alone) {
  br::Map x = read(br::Family::v29), y = read(br::Family::v29);
  br::cells_as_brushes(x);
  br::cells_as_brushes(y);
  CHECK(x.brushes.size() == y.brushes.size());
  CHECK(x.brush_sides.size() == y.brush_sides.size());
  bool same = x.planes.size() == y.planes.size();
  for (size_t i = 0; same && i < x.planes.size(); ++i)
    for (int k = 0; k < 3; ++k) same = same && x.planes[i].n[k] == y.planes[i].n[k];
  CHECK(same);
  const size_t had = x.brushes.size();
  CHECK(br::cells_as_brushes(x).brushes == 0u);  // it has brushes now
  CHECK(x.brushes.size() == had);
}
