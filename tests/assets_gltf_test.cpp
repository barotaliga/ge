// engine/assets/gltf.hpp and gltf_write.hpp, held by the engine's own checks:
//
//   * THE ROUND TRIP: the test model written as a .glb and loaded back poses as the model did,
//     at every clip and time the test sweeps, its names, materials and clips kept; a write is
//     the same bytes twice.
//   * THE FRAME: a hand-written .gltf (a rigid triangle on a node moved +1 m along glTF's x,
//     with no normals) loads mirrored in x, faceted, its front toward +z as written.
//   * THE SAMPLERS: STEP holds its key, LINEAR blends, a node's matrix is split into its rest.
//   * THE ERRORS: bytes that are not glTF, and a mesh of points, fail with what, where and
//     the next action.
//
// ge:summary Tests for the glTF loader and writer: round trip, frame, samplers, errors
// ge:keywords test assets gltf glb loader writer round-trip mirror faceted step linear matrix
// ge:skill assets testing
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/gltf_write.hpp"
#include "assets/pose.hpp"
#include "assets/test_model.hpp"
#include "check.hpp"

namespace {

namespace a = ge::assets;

std::vector<float> posed(const a::Model& m, int clip, double t) {
  std::vector<a::Trs> local(m.joints.size());
  std::vector<a::Affine> global(m.joints.size()), palette(m.joints.size());
  if (clip < 0) {
    a::rest_pose(m, local);
  } else {
    a::sample_clip(m, m.clips[static_cast<size_t>(clip)], t, false, local);
  }
  a::pose_globals(m, local, global);
  a::skin_palette(m, global, palette);
  std::vector<float> pos(size_t{m.meshes[0].vertex_count()} * 3), nrm(pos.size());
  a::skin_mesh(m.meshes[0], palette, pos, nrm);
  pos.insert(pos.end(), nrm.begin(), nrm.end());
  return pos;
}

std::string base64(const std::vector<uint8_t>& b) {
  static const char* k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string s;
  for (size_t i = 0; i < b.size(); i += 3) {
    const uint32_t n = (uint32_t{b[i]} << 16) | (i + 1 < b.size() ? uint32_t{b[i + 1]} << 8 : 0) |
                       (i + 2 < b.size() ? b[i + 2] : 0);
    s += k[(n >> 18) & 63];
    s += k[(n >> 12) & 63];
    s += i + 1 < b.size() ? k[(n >> 6) & 63] : '=';
    s += i + 2 < b.size() ? k[n & 63] : '=';
  }
  return s;
}

void put(std::vector<uint8_t>& b, std::initializer_list<float> v) {
  for (float f : v) {
    uint8_t x[4];
    std::memcpy(x, &f, 4);
    b.insert(b.end(), x, x + 4);
  }
}

a::LoadResult load_text(const std::string& json) {
  return a::load_gltf({reinterpret_cast<const uint8_t*>(json.data()), json.size()}, "test.gltf");
}

// A rigid triangle (no normals) on node 0, moved +1 m along glTF's x; node 1 under it placed by a
// matrix; two clips moving node 0 along x by 1 m over 1 s, one STEP and one LINEAR.
std::string triangle_gltf(const char* mode = "4") {
  std::vector<uint8_t> buf;
  put(buf, {0, 0, 0, 1, 0, 0, 0, 1, 0});  // 0: three positions
  put(buf, {0, 1});                       // 36: two times
  put(buf, {0, 0, 0, 1, 0, 0});           // 44: two translations
  return std::string(R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
  "nodes":[{"name":"tri","mesh":0,"translation":[1,0,0],"children":[1]},
           {"name":"lifted","matrix":[1,0,0,0, 0,1,0,0, 0,0,1,0, 0,2,0,1]}],
  "meshes":[{"primitives":[{"attributes":{"POSITION":0},"mode":)") +
         mode + R"(}]}],
  "animations":[
    {"name":"step","samplers":[{"input":1,"output":2,"interpolation":"STEP"}],
     "channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
    {"name":"slide","samplers":[{"input":1,"output":2,"interpolation":"LINEAR"}],
     "channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
  "buffers":[{"byteLength":68,"uri":"data:application/octet-stream;base64,)" +
         base64(buf) + R"("}],
  "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},
                 {"buffer":0,"byteOffset":36,"byteLength":8},
                 {"buffer":0,"byteOffset":44,"byteLength":24}],
  "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",
                "min":[0,0,0],"max":[1,1,0]},
               {"bufferView":1,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},
               {"bufferView":2,"componentType":5126,"count":2,"type":"VEC3"}]})";
}

}  // namespace

TEST(test_model_round_trips_through_a_glb) {
  const a::Model m = a::make_test_model();
  const std::vector<uint8_t> glb = a::write_glb(m);
  CHECK(glb == a::write_glb(m));
  const a::LoadResult r = a::load_gltf(glb, "test_model.glb");
  CHECK_MSG(r.ok, r.error);
  if (!r.ok) return;
  CHECK(r.notes.empty());
  const a::Model& l = r.model;
  CHECK(l.joints.size() == m.joints.size() + 1);  // and the mesh's node
  for (size_t j = 0; j < m.joints.size(); ++j) {
    CHECK(l.joints[j].name == m.joints[j].name && l.joints[j].parent == m.joints[j].parent);
  }
  CHECK(l.meshes.size() == 1 && l.meshes[0].name == "limb");
  CHECK(l.meshes[0].vertex_count() == m.meshes[0].vertex_count());
  CHECK(l.meshes[0].indices.size() == m.meshes[0].indices.size());
  CHECK(l.materials.size() == 2 && l.materials[1].name == "fin");
  CHECK(l.clips.size() == 2 && l.clips[1].name == "twist" &&
        l.clips[1].frames == m.clips[1].frames);
  // The primitives come back grouped by material, so compare the posed vertices as sets of
  // triangles: here the order is kept (the tube's part comes first), so vertex for vertex.
  for (int clip = -1; clip < 2; ++clip) {
    for (double t : {0.0, 0.2, 0.5, 0.9}) {
      const std::vector<float> want = posed(m, clip, t), got = posed(l, clip, t);
      float worst = 0;
      for (size_t i = 0; i < want.size(); ++i) worst = std::max(worst, std::fabs(want[i] - got[i]));
      CHECK_MSG(worst < 1e-5f, "clip " + std::to_string(clip) + " at " + std::to_string(t) +
                                   " poses " + std::to_string(worst) + " away after a round trip");
    }
  }
}

TEST(gltf_is_mirrored_into_the_engine_frame_and_made_faceted) {
  const a::LoadResult r = load_text(triangle_gltf());
  CHECK_MSG(r.ok, r.error);
  if (!r.ok) return;
  const a::Model& m = r.model;
  CHECK(m.joints.size() == 2 && m.joints[1].parent == 0);
  CHECK(m.joints[0].rest.t[0] == -1.0f);                   // +1 m along glTF's x is left
  CHECK(std::fabs(m.joints[1].rest.t[1] - 2.0f) < 1e-6f);  // the matrix's lift
  CHECK(m.meshes.size() == 1 && m.meshes[0].name == "tri");
  const a::Mesh& mesh = m.meshes[0];
  CHECK(mesh.vertex_count() == 3 && mesh.indices.size() == 3);
  CHECK(m.materials.size() == 1 && m.materials[0].name == "default");
  // A rigid part is bound whole to its node, at its place in the model.
  std::vector<float> pos = posed(m, -1, 0);
  const float want[9] = {-1, 0, 0, -1, 1, 0, -2, 0, 0};  // the triangle's last two swapped
  for (int i = 0; i < 9; ++i) CHECK(std::fabs(pos[i] - want[i]) < 1e-6f);
  // Its front stays toward +z: the face's normal, and (b - a) x (c - a).
  for (int v = 0; v < 3; ++v) CHECK(std::fabs(pos[9 + v * 3 + 2] - 1) < 1e-6f);
  const float u[3] = {pos[3] - pos[0], pos[4] - pos[1], pos[5] - pos[2]};
  const float w[3] = {pos[6] - pos[0], pos[7] - pos[1], pos[8] - pos[2]};
  CHECK(u[0] * w[1] - u[1] * w[0] > 0);
}

TEST(step_holds_and_linear_blends) {
  const a::LoadResult r = load_text(triangle_gltf());
  CHECK_MSG(r.ok, r.error);
  if (!r.ok) return;
  const a::Model& m = r.model;
  const int step = a::find_clip(m, "step"), slide = a::find_clip(m, "slide");
  CHECK(step >= 0 && slide >= 0);
  if (step < 0 || slide < 0) return;
  CHECK(m.clips[static_cast<size_t>(slide)].frames == 61);
  std::vector<a::Trs> local(m.joints.size());
  a::sample_clip(m, m.clips[static_cast<size_t>(step)], 0.5, false, local);
  CHECK(local[0].t[0] == 0.0f);
  a::sample_clip(m, m.clips[static_cast<size_t>(slide)], 0.5, false, local);
  CHECK(std::fabs(local[0].t[0] + 0.5f) < 1e-6f);  // half of glTF's +1, mirrored
  a::sample_clip(m, m.clips[static_cast<size_t>(slide)], 1.0, false, local);
  CHECK(std::fabs(local[0].t[0] + 1.0f) < 1e-6f);
}

TEST(what_is_not_gltf_fails_with_the_next_action) {
  const std::string junk = "not a model";
  const a::LoadResult r = load_text(junk);
  CHECK(!r.ok && r.error.rfind("assets: test.gltf: ", 0) == 0);
  CHECK(r.error.find("next:") != std::string::npos);
  const a::LoadResult points = load_text(triangle_gltf("0"));
  CHECK(!points.ok && points.error.find("points, lines or strips") != std::string::npos);
  CHECK(points.error.find("next: triangulate") != std::string::npos);
  const a::LoadResult missing = a::load_gltf_file("no/such/model.glb");
  CHECK(!missing.ok && missing.error.find("cannot be opened") != std::string::npos);
}
