// The morph model's glTF writer (plugins/morph/glb.hpp): the fixture model written and loaded
// back by the engine's own glTF loader (which validates the file) keeps its vertices, its
// positions turned into the engine's frame and scaled, its skin's colour, and every frame as a
// morph target with its clips; the clips follow the frames' stems and groups.
//
// ge:summary Tests the morph glTF writer: a loadable file, positions, skin, targets, clips
// ge:keywords test morph gltf glb write morph-targets clip stem group skin load
// ge:skill morph testing
#include <cmath>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "check.hpp"
#include "lab/png.hpp"
#include "lab/raster.hpp"
#include "morph/glb.hpp"
#include "morph/write.hpp"

namespace mo = ge::morph;

namespace {

bool noted(const ge::assets::LoadResult& r, const std::string& what) {
  for (const std::string& n : r.notes) {
    if (n.find(what) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

TEST(clips_follow_stems_and_groups) {
  mo::Model m = mo::sample_model();  // "stand" alone, then "grow1" and "grow2" in a group
  mo::Clips c = mo::clips_of(m);
  CHECK(c.names.size() == 2 && c.names[0] == "stand" && c.names[1] == "grow");
  CHECK(c.of_frame == std::vector<int>({0, 1, 1}));
  // Lone frames with one stem make one clip; a stem met again later gets a clip of its own.
  for (mo::Frame& f : m.frames) f.interval = 0;
  m.frames[1].group = 1;
  m.frames[2].group = 2;
  m.frames.push_back(m.frames[0]);
  c = mo::clips_of(m);
  CHECK(c.names == std::vector<std::string>({"stand", "grow", "stand_2"}));
  CHECK(c.of_frame == std::vector<int>({0, 1, 1, 2}));
}

TEST(written_file_loads_with_its_targets_skin_and_frame) {
  const mo::Model m = mo::sample_model();
  ge::lab::Picture red = ge::lab::create_picture(8, 8, {230, 10, 10});
  mo::GlbOptions opts;
  opts.unit_m = 0.5f;
  const std::vector<uint8_t> glb =
      mo::write_glb(m, "sample", {{ge::lab::encode_png(red), {}}}, opts);
  const ge::assets::LoadResult r = ge::assets::load_gltf(glb, "sample.glb");
  CHECK_MSG(r.ok, r.error);
  CHECK(noted(r, "morph targets") && noted(r, "morph weights"));
  CHECK(r.model.meshes.size() == 1);
  const ge::assets::Mesh& mesh = r.model.meshes[0];
  CHECK(mesh.vertex_count() == m.vertex_count() && mesh.indices.size() == m.indices.size());
  // Every loaded vertex is a file vertex of the first frame: forward (x) to the engine's +z, left
  // (y) to its -x (glTF's +x is the asset's left, the loader mirrors it), up (z) to +y, halved.
  const std::vector<float>& p = m.frames[0].positions;
  for (size_t v = 0; v < mesh.vertex_count(); v++) {
    bool found = false;
    for (size_t w = 0; w < m.vertex_count() && !found; w++) {
      found = std::fabs(mesh.positions[v * 3] + 0.5f * p[w * 3 + 1]) < 1e-5f &&
              std::fabs(mesh.positions[v * 3 + 1] - 0.5f * p[w * 3 + 2]) < 1e-5f &&
              std::fabs(mesh.positions[v * 3 + 2] - 0.5f * p[w * 3]) < 1e-5f;
    }
    CHECK(found);
  }
  // The skin's colour reached the material.
  bool reddish = false;
  for (const ge::assets::Material& mat : r.model.materials)
    reddish = reddish || (mat.rgb[0] > 0.6f && mat.rgb[1] < 0.05f && mat.rgb[2] < 0.05f);
  CHECK(reddish);
}

TEST(written_file_without_skins_still_loads) {
  mo::Model m = mo::sample_model();
  const ge::assets::LoadResult r = ge::assets::load_gltf(mo::write_glb(m, "bare", {}), "bare.glb");
  CHECK_MSG(r.ok, r.error);
}
