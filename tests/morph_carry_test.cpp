// A figure taken apart and a model carried in it (plugins/morph/carry.hpp) on a made figure: a body
// of four points that never moves and a gun of four that turns a quarter about the up axis and
// steps forward in the second frame. The pieces come out by size, the fit finds the quarter turn
// and the step, the cut keeps only the body, and a carried model rides the gun's motion.
//
// ge:summary Tests a morph figure's pieces, the cut, the rigid fit and the carried model
// ge:keywords test morph carry pieces keep_vertices fit_rigid carried vwep
// ge:skill morph testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "morph/carry.hpp"

namespace mo = ge::morph;

namespace {

// Four points: a tetrahedron at `at`, `size` across.
void tetra(std::vector<float>& p, float x, float y, float z, float size) {
  const float pts[4][3] = {{0, 0, 0}, {size, 0, 0}, {0, size, 0}, {0, 0, size}};
  for (const auto& q : pts) p.insert(p.end(), {x + q[0], y + q[1], z + q[2]});
}

// The gun's points turned a quarter about +z (x to y) and moved by (5, 0, 1).
void turned(std::vector<float>& p, const std::vector<float>& from) {
  for (size_t v = 0; v < from.size(); v += 3)
    p.insert(p.end(), {-from[v + 1] + 5, from[v], from[v + 2] + 1});
}

mo::Model figure() {
  mo::Model m;
  for (int t = 0; t < 2; t++) {  // body 0..4 (five points: a tetrahedron and a fan), gun 5..8
    const uint32_t b = t == 0 ? 0 : 5;
    m.indices.insert(m.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3, b, b + 3, b + 1});
  }
  m.indices.insert(m.indices.end(), {1, 2, 4});
  m.st.assign(size_t{9} * 2, 0.5f);
  mo::Frame a, b;
  a.name = "stand";
  b.name = "aim";
  tetra(a.positions, 0, 0, 0, 10);
  a.positions.insert(a.positions.end(), {5, 5, 0});
  std::vector<float> gun;
  tetra(gun, 2, 1, 3, 4);
  a.positions.insert(a.positions.end(), gun.begin(), gun.end());
  b.positions.assign(a.positions.begin(), a.positions.begin() + 15);
  turned(b.positions, gun);
  m.frames = {a, b};
  return m;
}

}  // namespace

TEST(pieces_are_numbered_by_size) {
  const mo::Model m = figure();
  const std::vector<int> p = mo::pieces(m);
  CHECK(p.size() == 9);
  for (int v = 0; v < 5; v++) CHECK(p[size_t(v)] == 0);
  for (int v = 5; v < 9; v++) CHECK(p[size_t(v)] == 1);
  CHECK(mo::piece_vertices(p, 1).size() == 4);
}

TEST(the_cut_keeps_only_the_marked_vertices) {
  const mo::Model m = figure();
  const std::vector<int> p = mo::pieces(m);
  std::vector<bool> keep(p.size());
  for (size_t v = 0; v < p.size(); v++) keep[v] = p[v] == 0;
  const mo::Model body = mo::keep_vertices(m, keep);
  CHECK(body.vertex_count() == 5);
  CHECK(body.indices.size() == 12);
  CHECK(body.frames.size() == 2);
  CHECK(body.frames[1].positions.size() == 15);
  for (const uint32_t i : body.indices) CHECK(i < 5);
}

TEST(the_fit_finds_the_quarter_turn_and_the_step) {
  const mo::Model m = figure();
  const std::vector<uint32_t> gun = mo::piece_vertices(mo::pieces(m), 1);
  const mo::Rigid r = mo::fit_rigid(m, gun, 0, 1);
  const double want[9] = {0, -1, 0, 1, 0, 0, 0, 0, 1};
  for (int i = 0; i < 9; i++) CHECK(std::fabs(r.r[i] - want[i]) < 1e-9);
  CHECK(std::fabs(r.t[0] - 5) < 1e-9 && std::fabs(r.t[1]) < 1e-9 && std::fabs(r.t[2] - 1) < 1e-9);
  const mo::Rigid same = mo::fit_rigid(m, gun, 0, 0);
  for (int i = 0; i < 9; i++) CHECK(std::fabs(same.r[i] - (i % 4 == 0 ? 1 : 0)) < 1e-9);
}

TEST(a_carried_model_rides_the_grip) {
  const mo::Model m = figure();
  const std::vector<uint32_t> gun = mo::piece_vertices(mo::pieces(m), 1);
  mo::Model held;
  held.indices = {0, 1, 2};
  held.st.assign(6, 0.25f);
  mo::Frame f;
  f.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  held.frames = {f};
  mo::Rigid mount;
  mount.t[0] = 2;  // held two units along +x of the gun's place in the reference frame
  const mo::Model c = mo::carried(held, m, gun, 0, mount);
  CHECK(c.frames.size() == 2);
  CHECK(c.frames[1].name == "aim");
  CHECK(c.indices == held.indices);
  // Frame 0: the mount alone. Frame 1: turned a quarter (x to y) and moved by (5, 0, 1).
  const std::vector<float> want0 = {2, 0, 0, 3, 0, 0, 2, 1, 0};
  const std::vector<float> want1 = {5, 2, 1, 5, 3, 1, 4, 2, 1};
  for (size_t i = 0; i < 9; i++) {
    CHECK(std::fabs(c.frames[0].positions[i] - want0[i]) < 1e-5f);
    CHECK(std::fabs(c.frames[1].positions[i] - want1[i]) < 1e-5f);
  }
  CHECK(c.frames[1].min[0] == 4 && c.frames[1].max[1] == 3);
}

TEST(the_surface_ends_are_its_limbs_tips) {
  // A star of thin triangles from a centre (0, 1): tips 2 (5 away), 3 (9 away), 4 (3 away).
  mo::Model m;
  mo::Frame f;
  f.positions = {0, 0, 0, 0, 0, 1, 5, 0, 0, 0, 9, 0, -3, 0, 0};
  m.frames = {f};
  m.indices = {0, 1, 2, 0, 1, 3, 0, 1, 4};
  m.st.assign(10, 0.0f);
  const std::vector<double> d = mo::surface_distance(m, 0, 0);
  CHECK(d[0] == 0 && std::fabs(d[3] - 9) < 1e-9 && std::fabs(d[1] - 1) < 1e-9);
  const std::vector<uint32_t> ends = mo::surface_ends(m, 0, 0);
  CHECK(ends.size() == 3);  // the three tips
  CHECK(ends[0] == 3 && ends[1] == 2 && ends[2] == 4);
}

TEST(carried_by_places_each_frame_its_own_way) {
  const mo::Model fig = figure();
  mo::Model held;
  mo::Frame f;
  f.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  held.frames = {f};
  held.indices = {0, 1, 2};
  held.st.assign(6, 0.0f);
  std::vector<mo::Rigid> each(1);
  each[0].t[0] = 5;  // one given: every frame takes it
  const mo::Model out = mo::carried_by(held, fig, each);
  CHECK(out.frames.size() == fig.frames.size());
  CHECK(out.frames.back().positions[0] == 5.0f && out.frames.back().name == fig.frames.back().name);
}
