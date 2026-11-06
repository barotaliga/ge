// The skeletal model file (engine/assets/iqm.hpp):
//
//   * A ROUND TRIP: the test model written and read back has its joints, rest poses, meshes,
//     materials, weights and clips, and posed by every clip at a sweep of times it stands where
//     the original does (within the 16-bit frames' step).
//   * THE FRAME: a point at the engine's (1, 2, 3) is the file's (3, -1, 2), and a file's unit
//     comes in times `metres`.
//   * THE WINDING follows the normals: with every normal turned inward the triangles turn over too.
//   * A FILE WITHOUT JOINTS gets one root and every vertex bound to it.
//   * FAULTS: a file that is not one, cut short, a joint before its parent, a triangle outside its
//     mesh; each names the file and the next step, and gives no model.
//
// ge:summary Tests for the .iqm reader and writer: round trip, frame, winding, root, faults
// ge:keywords test assets iqm load_iqm write_iqm round-trip frame winding faults skeletal-model
// ge:skill assets testing
#include <bit>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "assets/iqm.hpp"
#include "assets/pose.hpp"
#include "assets/test_model.hpp"
#include "check.hpp"

namespace {

namespace a = ge::assets;

uint32_t u32(const std::vector<uint8_t>& b, size_t at) {
  return uint32_t{b[at]} | uint32_t{b[at + 1]} << 8 | uint32_t{b[at + 2]} << 16 |
         uint32_t{b[at + 3]} << 24;
}
void put(std::vector<uint8_t>& b, size_t at, uint32_t v) {
  for (int k = 0; k < 4; ++k) b[at + k] = static_cast<uint8_t>(v >> (8 * k));
}
// The header's fields by their place after the magic.
size_t field(int n) { return 16 + static_cast<size_t>(n) * 4; }
constexpr int kOfsVertexArrays = 9, kOfsTriangles = 11, kNumJoints = 13, kOfsJoints = 14;

std::vector<float> posed(const a::Model& m, int clip, double t) {
  std::vector<a::Trs> local(m.joints.size());
  std::vector<a::Affine> global(m.joints.size()), palette(m.joints.size());
  if (clip < 0)
    a::rest_pose(m, local);
  else
    a::sample_clip(m, m.clips[static_cast<size_t>(clip)], t, false, local);
  a::pose_globals(m, local, global);
  a::skin_palette(m, global, palette);
  std::vector<float> out;
  for (const a::Mesh& mesh : m.meshes) {
    std::vector<float> p(mesh.positions.size()), n(mesh.normals.size());
    a::skin_mesh(mesh, palette, p, n);
    out.insert(out.end(), p.begin(), p.end());
  }
  return out;
}

float most_apart(const std::vector<float>& x, const std::vector<float>& y) {
  float worst = x.size() == y.size() ? 0.0f : 1e9f;
  for (size_t i = 0; i < x.size() && i < y.size(); ++i)
    worst = std::max(worst, std::abs(x[i] - y[i]));
  return worst;
}

}  // namespace

TEST(the_test_model_written_and_read_back_is_the_same_model) {
  const a::Model m = a::make_test_model();
  const std::vector<uint8_t> file = a::write_iqm(m);
  const a::LoadResult r = a::load_iqm(file, "limb.iqm");
  CHECK_MSG(r.ok, r.error);
  const a::Model& b = r.model;
  CHECK(b.joints.size() == m.joints.size());
  for (size_t j = 0; j < m.joints.size() && j < b.joints.size(); ++j) {
    CHECK(b.joints[j].name == m.joints[j].name && b.joints[j].parent == m.joints[j].parent);
    for (int k = 0; k < 3; ++k)
      CHECK(std::abs(b.joints[j].rest.t[k] - m.joints[j].rest.t[k]) < 1e-6f);
    for (int k = 0; k < 12; ++k)
      CHECK(std::abs(b.joints[j].inverse_bind.m[k] - m.joints[j].inverse_bind.m[k]) < 1e-5f);
  }
  CHECK(b.meshes.size() == m.meshes.size() && b.materials.size() == m.materials.size());
  for (size_t i = 0; i < m.meshes.size() && i < b.meshes.size(); ++i) {
    CHECK(b.meshes[i].name == m.meshes[i].name);
    CHECK(b.meshes[i].indices == m.meshes[i].indices);  // the winding kept
    CHECK(most_apart(b.meshes[i].positions, m.meshes[i].positions) < 1e-6f);
    CHECK(most_apart(b.meshes[i].normals, m.meshes[i].normals) < 1e-6f);
    CHECK(most_apart(b.meshes[i].weights, m.meshes[i].weights) <= 1.0f / 255);
    CHECK(b.meshes[i].parts == m.meshes[i].parts);
    // Each influence that weighs anything names the same joint (an empty slot reads as joint 0).
    for (size_t k = 0; k < m.meshes[i].weights.size() && k < b.meshes[i].joints.size(); ++k)
      if (m.meshes[i].weights[k] > 1.0f / 255)
        CHECK(b.meshes[i].joints[k] == m.meshes[i].joints[k]);
  }
  CHECK(b.clips.size() == m.clips.size());
  for (size_t c = 0; c < m.clips.size() && c < b.clips.size(); ++c) {
    CHECK(b.clips[c].name == m.clips[c].name && b.clips[c].frames == m.clips[c].frames);
    const double end = m.clips[c].duration_s();
    for (int k = 0; k <= 8; ++k) {
      const double t = end * k / 8;
      CHECK_MSG(
          most_apart(posed(b, static_cast<int>(c), t), posed(m, static_cast<int>(c), t)) < 1e-3f,
          m.clips[c].name + " at " + std::to_string(t));
    }
  }
  CHECK(most_apart(posed(b, -1, 0), posed(m, -1, 0)) < 1e-5f);
}

TEST(a_point_comes_in_turned_into_the_engine_s_frame_and_scaled_by_the_unit) {
  a::Model m = a::make_test_model();
  m.clips.clear();
  m.meshes.resize(1);
  m.meshes[0].positions[0] = 1;
  m.meshes[0].positions[1] = 2;
  m.meshes[0].positions[2] = 3;
  const std::vector<uint8_t> file = a::write_iqm(m);
  // The first array is the positions: the file's x is the engine's z, its y the engine's -x.
  const size_t at = u32(file, u32(file, field(kOfsVertexArrays)) + 16);
  CHECK(std::bit_cast<float>(u32(file, at)) == 3.0f);
  CHECK(std::bit_cast<float>(u32(file, at + 4)) == -1.0f);
  CHECK(std::bit_cast<float>(u32(file, at + 8)) == 2.0f);
  const a::LoadResult r = a::load_iqm(file, "unit.iqm", {.metres = 0.025});
  CHECK_MSG(r.ok, r.error);
  CHECK(r.ok && std::abs(r.model.meshes[0].positions[2] - 0.075f) < 1e-7f);
  CHECK(r.ok && std::abs(r.model.joints[1].rest.t[1] - m.joints[1].rest.t[1] * 0.025f) < 1e-7f);
}

TEST(the_triangles_face_the_way_the_normals_say) {
  a::Model m = a::make_test_model();
  for (a::Mesh& mesh : m.meshes)
    for (float& n : mesh.normals) n = -n;
  const a::LoadResult r = a::load_iqm(a::write_iqm(m), "inside_out.iqm");
  CHECK_MSG(r.ok, r.error);
  const std::vector<uint32_t>& got = r.model.meshes[0].indices;
  const std::vector<uint32_t>& was = m.meshes[0].indices;
  CHECK(got.size() == was.size());
  for (size_t t = 0; t + 2 < got.size() && t + 2 < was.size(); t += 3)
    CHECK(got[t] == was[t] && got[t + 1] == was[t + 2] && got[t + 2] == was[t + 1]);
}

TEST(a_file_without_joints_is_bound_to_one_root) {
  a::Model m = a::make_test_model();
  m.clips.clear();
  std::vector<uint8_t> file = a::write_iqm(m);
  put(file, field(kNumJoints), 0);  // the joints gone, the vertices' blend arrays left over
  const a::LoadResult r = a::load_iqm(file, "rigid.iqm");
  CHECK_MSG(r.ok, r.error);
  CHECK(r.ok && r.model.joints.size() == 1 && r.model.joints[0].parent == -1);
  for (const a::Mesh& mesh : r.model.meshes)
    for (size_t v = 0; v < mesh.vertex_count(); ++v)
      CHECK(mesh.joints[v * 4] == 0 && mesh.weights[v * 4] == 1.0f);
}

TEST(a_bad_file_is_refused_with_its_name_and_the_next_step) {
  const std::vector<uint8_t> good = a::write_iqm(a::make_test_model());
  std::vector<std::vector<uint8_t>> bad;
  bad.push_back({'n', 'o', 't', ' ', 'a', ' ', 'm', 'o', 'd', 'e', 'l'});
  bad.emplace_back(good.begin(), good.begin() + static_cast<long>(good.size() / 2));
  {
    std::vector<uint8_t> b = good;  // the second joint's parent made itself
    put(b, u32(b, field(kOfsJoints)) + 48 + 4, 1);
    bad.push_back(b);
  }
  {
    std::vector<uint8_t> b = good;  // a triangle naming a vertex of the next mesh
    put(b, u32(b, field(kOfsTriangles)), 100000);
    bad.push_back(b);
  }
  {
    std::vector<uint8_t> b = good;
    put(b, 16, 1);  // version 1
    bad.push_back(b);
  }
  for (const auto& b : bad) {
    const a::LoadResult r = a::load_iqm(b, "mod/progs/bad.iqm");
    CHECK(!r.ok && r.model.meshes.empty());
    CHECK_MSG(
        r.error.starts_with("mod/progs/bad.iqm: ") && r.error.find("next:") != std::string::npos,
        r.error);
  }
}
