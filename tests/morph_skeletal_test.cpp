// A skeletal model baked into a morph model (plugins/morph/skeletal.hpp):
//
//   * THE FRAMES: the test model's two clips are two frame groups, each its poses 1 / fps apart
//     from its start to its end; every frame's positions are the model posed there, in the
//     classic file's frame and units.
//   * THE TRIANGLES keep their outside: a frame's faces point the way the engine's do, turned.
//   * THE SKIN: texture coordinates moved into each material's rect; one grey stand-in skin.
//   * A model with no clips is one frame at rest; a bake past the frame cap is refused.
//
// ge:summary Tests for the skeletal bake: frame groups, poses, winding, the atlas rects, faults
// ge:keywords test morph skeletal bake_skeletal frame-group atlas winding
// ge:skill morph testing
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "assets/pose.hpp"
#include "assets/test_model.hpp"
#include "check.hpp"
#include "morph/skeletal.hpp"

namespace {

namespace a = ge::assets;
namespace mo = ge::morph;

// The test model's mesh posed by `clip` at `t`, in the engine's frame.
std::vector<float> engine_pose(const a::Model& m, int clip, double t) {
  std::vector<a::Trs> local(m.joints.size());
  std::vector<a::Affine> global(m.joints.size()), palette(m.joints.size());
  a::sample_clip(m, m.clips[static_cast<size_t>(clip)], t, false, local);
  a::pose_globals(m, local, global);
  a::skin_palette(m, global, palette);
  std::vector<float> p(m.meshes[0].positions.size()), n(p.size());
  a::skin_mesh(m.meshes[0], palette, p, n);
  return p;
}

}  // namespace

TEST(each_clip_is_a_frame_group_of_its_poses_in_the_file_s_frame) {
  const a::Model m = a::make_test_model();
  mo::Model out;
  std::string error;
  CHECK_MSG(mo::bake_skeletal(m, "limb", {.fps = 10, .metres = 0.025}, &out, &error), error);
  int groups[2] = {0, 0};
  for (const mo::Frame& f : out.frames)
    if (f.group >= 0 && f.group < 2) groups[f.group]++;
  // wave: 1 s at 10 a second, its ends both kept unless they meet; twist: 0.5 s.
  CHECK(groups[0] >= 10 && groups[0] <= 11 && groups[1] == 6);
  CHECK(out.frames.size() == size_t(groups[0] + groups[1]));
  CHECK(!out.frames.empty() && std::abs(out.frames[1].interval - 0.1f) < 1e-6f);
  // Frame 3 of wave is the engine's pose at 0.3 s: (x, y, z) m is (z, -x, y) / 0.025 units.
  const std::vector<float> e = engine_pose(m, 0, 0.3);
  const std::vector<float>& f = out.frames[3].positions;
  CHECK(f.size() >= e.size());
  for (size_t v = 0; v * 3 < e.size() && v * 3 < f.size(); ++v) {
    CHECK(std::abs(f[v * 3] - e[v * 3 + 2] / 0.025f) < 1e-3f);
    CHECK(std::abs(f[v * 3 + 1] + e[v * 3] / 0.025f) < 1e-3f);
    CHECK(std::abs(f[v * 3 + 2] - e[v * 3 + 1] / 0.025f) < 1e-3f);
  }
  CHECK(out.skins.size() == 1 && out.skins[0].indices.size() == 64 && out.skins[0].indices[0] == 8);
  CHECK(out.radius > 30);
}

TEST(the_triangles_keep_their_outside_through_the_mirror) {
  const a::Model m = a::make_test_model();
  mo::Model out;
  std::string error;
  CHECK_MSG(mo::bake_skeletal(m, "limb", {}, &out, &error), error);
  // At rest the tube's faces point away from its axis (the file's z): their normals' sideways
  // part points the way the face's middle lies from the axis.
  std::vector<a::Trs> local(m.joints.size());
  a::rest_pose(m, local);
  int out_ward = 0, in_ward = 0;
  const std::vector<float>& p = out.frames[0].positions;
  for (size_t t = 0; t + 2 < out.indices.size(); t += 3) {
    const float* x = &p[out.indices[t] * 3];
    const float* y = &p[out.indices[t + 1] * 3];
    const float* z = &p[out.indices[t + 2] * 3];
    const float e1[3] = {y[0] - x[0], y[1] - x[1], y[2] - x[2]};
    const float e2[3] = {z[0] - x[0], z[1] - x[1], z[2] - x[2]};
    const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                        e1[0] * e2[1] - e1[1] * e2[0]};
    const float c[2] = {(x[0] + y[0] + z[0]) / 3, (x[1] + y[1] + z[1]) / 3};
    const float d = n[0] * c[0] + n[1] * c[1];
    if (d > 0) out_ward++;
    if (d < 0) in_ward++;
  }
  CHECK_MSG(out_ward > in_ward * 3,
            std::to_string(out_ward) + " out, " + std::to_string(in_ward) + " in");
}

TEST(the_coordinates_move_into_their_material_s_rect) {
  a::Model m = a::make_test_model();
  m.meshes[0].uv.assign(size_t{m.meshes[0].vertex_count()} * 2, 0.5f);
  const std::array<float, 4> rects[] = {{0, 0, 1, 0.25f}, {0, 0.25f, 0.5f, 1}};
  mo::Model out;
  std::string error;
  CHECK_MSG(mo::bake_skeletal(m, "limb", {.rects = rects}, &out, &error), error);
  for (uint32_t v = 0; v < out.vertex_count(); ++v) {
    const uint8_t part = m.meshes[0].parts[v];
    const float u = out.st[v * 2], t = out.st[v * 2 + 1];
    if (part == 0) CHECK(std::abs(u - 0.5f) < 1e-6f && std::abs(t - 0.125f) < 1e-6f);
    if (part == 1) CHECK(std::abs(u - 0.25f) < 1e-6f && std::abs(t - 0.625f) < 1e-6f);
  }
}

TEST(no_clip_is_one_frame_at_rest_and_too_many_frames_are_refused) {
  a::Model m = a::make_test_model();
  m.clips.clear();
  mo::Model out;
  std::string error;
  CHECK_MSG(mo::bake_skeletal(m, "still", {}, &out, &error), error);
  CHECK(out.frames.size() == 1 && out.frames[0].interval == 0);
  const a::Model moving = a::make_test_model();
  CHECK(!mo::bake_skeletal(moving, "fast", {.fps = 1e5}, &out, &error));
  CHECK_MSG(error.starts_with("fast: ") && error.find("next:") != std::string::npos, error);
  CHECK(out.frames.empty());
}
