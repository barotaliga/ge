// Smooth normals and the optional rounding (plugins/morph/smooth.hpp) on the fixture octahedron:
// every frame's vertex normals are unit and point outward, and the copies of a position on the
// seam share one; rounding keeps the corners where they were, cuts each triangle into level²,
// meets itself along every edge without a crack, leaves an edge sharper than the crease angle
// straight (the octahedron stays flat at 60°) and bulges it at a wider angle.
//
// ge:summary Tests the morph model's frame normals and the point-normal rounding
// ge:keywords test morph smooth normals pn-triangles crease watertight corners seam
// ge:skill morph testing
#include <cmath>
#include <map>
#include <tuple>
#include <vector>

#include "check.hpp"
#include "morph/smooth.hpp"
#include "morph/write.hpp"

namespace mo = ge::morph;

namespace {

float taxicab(const std::vector<float>& p, size_t v, float size) {
  return (std::fabs(p[v * 3]) + std::fabs(p[v * 3 + 1]) + std::fabs(p[v * 3 + 2])) / size;
}

}  // namespace

TEST(frame_normals_are_unit_outward_and_shared_on_the_seam) {
  const mo::Model m = mo::sample_model();
  for (const mo::Frame& f : m.frames) {
    const std::vector<float> n = mo::frame_normals(m, f);
    CHECK(n.size() == f.positions.size());
    for (size_t v = 0; v < m.vertex_count(); v++) {
      const float len = std::sqrt(n[v * 3] * n[v * 3] + n[v * 3 + 1] * n[v * 3 + 1] +
                                  n[v * 3 + 2] * n[v * 3 + 2]);
      CHECK(std::fabs(len - 1.0f) < 1e-5f);
      const float out = n[v * 3] * f.positions[v * 3] + n[v * 3 + 1] * f.positions[v * 3 + 1] +
                        n[v * 3 + 2] * f.positions[v * 3 + 2];
      CHECK(out > 0);
      for (size_t w = 0; w < m.vertex_count(); w++) {
        if (m.source_vertex[w] == m.source_vertex[v]) {
          CHECK(n[w * 3] == n[v * 3] && n[w * 3 + 1] == n[v * 3 + 1] &&
                n[w * 3 + 2] == n[v * 3 + 2]);
        }
      }
    }
  }
}

TEST(rounding_keeps_corners_and_meets_along_edges) {
  const mo::Model m = mo::sample_model();
  mo::SmoothOptions o;
  o.level = 3;
  o.crease_deg = 120;  // round the octahedron's 90° edges
  const mo::Model r = mo::smooth_model(m, o);
  CHECK(r.indices.size() == m.indices.size() * 9);
  CHECK(r.frames.size() == m.frames.size());
  // Every original corner is still a vertex, unmoved.
  for (size_t v = 0; v < m.vertex_count(); v++) {
    bool found = false;
    for (size_t w = 0; w < r.vertex_count() && !found; w++) {
      found = r.frames[0].positions[w * 3] == m.frames[0].positions[v * 3] &&
              r.frames[0].positions[w * 3 + 1] == m.frames[0].positions[v * 3 + 1] &&
              r.frames[0].positions[w * 3 + 2] == m.frames[0].positions[v * 3 + 2];
    }
    CHECK(found);
  }
  // No crack: vertices sharing a source share a position in every frame.
  for (const mo::Frame& f : r.frames) {
    std::map<int, std::tuple<float, float, float>> at;
    for (size_t v = 0; v < r.vertex_count(); v++) {
      const auto p =
          std::make_tuple(f.positions[v * 3], f.positions[v * 3 + 1], f.positions[v * 3 + 2]);
      const auto [it, fresh] = at.try_emplace(r.source_vertex[v], p);
      CHECK(fresh || it->second == p);
    }
  }
  // Rounded: points inside the faces stand out past the flat faces.
  float most = 0;
  for (size_t v = 0; v < r.vertex_count(); v++)
    most = std::max(most, taxicab(r.frames[0].positions, v, 16));
  CHECK(most > 1.05f);
}

TEST(an_edge_past_the_crease_stays_straight) {
  const mo::Model m = mo::sample_model();
  mo::SmoothOptions o;
  o.level = 4;
  o.crease_deg = 60;  // the octahedron's corners differ by 90°: every edge is a crease
  const mo::Model r = mo::smooth_model(m, o);
  for (size_t f = 0; f < r.frames.size(); f++) {
    const float size = 16.0f * (f == 0 ? 1.0f : f == 1 ? 1.25f : 1.5f);
    for (size_t v = 0; v < r.vertex_count(); v++) {
      CHECK(std::fabs(taxicab(r.frames[f].positions, v, size) - 1.0f) < 1e-4f);
    }
  }
}
