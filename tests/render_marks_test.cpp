// engine/render/marks.hpp: a blast on the floor presses its scorch there and one off a wall's face
// presses it on the face and not on the floor, a blast in the open air leaves none; the burns
// carry each triangle of every live mark into its kind's and variant's cell of the atlas, a fresh
// scorch's embers at their hottest and cooled after, its soot left; the atlas is the decals'
// masks at the marks' size and the beams pass made for it has room for a pool's burns.
//
// ge:summary Tests for the marks drawn: the scorch pressed, the burns, the atlas
// ge:keywords test render marks decals scorch burns atlas ember soot press_scorch mark_burns
// ge:skill render testing
#include <vector>

#include "check.hpp"
#include "render/marks.hpp"

namespace {

namespace r = ge::render;
namespace gd = ge::detail;

// A floor to z = 9 and a wall's face rising from it to 4 m at z = 10, as a heightfield's step.
gd::TriMesh yard() {
  gd::TriMesh m;
  const auto quad = [&](gd::Vec3 a, gd::Vec3 b, gd::Vec3 c, gd::Vec3 d, uint16_t tag) {
    const auto base = static_cast<uint32_t>(m.pos.size());
    m.pos.insert(m.pos.end(), {a, b, c, d});
    m.idx.insert(m.idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    m.tag.insert(m.tag.end(), {tag, tag});
  };
  quad({-10, 0, -10}, {-10, 0, 9}, {10, 0, 9}, {10, 0, -10}, 0);
  quad({-10, 0, 9}, {-10, 4, 10}, {10, 4, 10}, {10, 0, 9}, 1);
  return m;
}

r::Blast blast(double x, double y, double z, const double n[3], double born) {
  r::Blast b;
  b.at[0] = x, b.at[1] = y, b.at[2] = z;
  b.on_surface = n != nullptr;
  if (n != nullptr)
    for (int k = 0; k < 3; ++k) b.normal[k] = n[k];
  b.seed = 5;
  b.born_s = born;
  return b;
}

}  // namespace

TEST(the_faces_point_out_of_the_yard) {
  const gd::TriMesh m = yard();
  CHECK(gd::normal_of(m, 0).y > 0.99);  // the floor, up
  const gd::Vec3 wall = gd::normal_of(m, 2);
  CHECK(wall.z < -0.9 && wall.y > 0);  // the wall's face, toward the yard
}

TEST(a_blast_presses_its_scorch_on_the_surface_it_went_off_against) {
  const gd::TriMesh m = yard();
  const gd::Receivers rec = gd::bin_receivers(m, 2);
  gd::DecalPool pool = gd::make_pool(4);
  const double up[3] = {0, 1, 0}, out[3] = {0, 0, -1};
  CHECK(r::press_scorch(pool, rec, blast(0, 0, 0, up, 1.0)) >= 0);
  const int wall = r::press_scorch(pool, rec, blast(4, 0.3, 9, out, 1.0));
  CHECK(wall >= 0);
  CHECK(r::press_scorch(pool, rec, blast(0, 2, 0, nullptr, 1.0)) < 0);  // in the open air
  CHECK(gd::live_count(pool) == 2);
  if (wall < 0) return;
  // The wall's mark lies on its face (up its slope, off the floor's plane), not on the floor.
  bool on_face = false;
  for (const gd::DecalVertex& v : gd::decal_verts(pool, static_cast<uint32_t>(wall))) {
    CHECK(v.pos.z >= 8.99);
    on_face = on_face || v.pos.y > 0.5;
  }
  CHECK(on_face);
}

TEST(the_burns_put_each_mark_in_its_cell_and_its_embers_cool) {
  const gd::TriMesh m = yard();
  const gd::Receivers rec = gd::bin_receivers(m, 2);
  gd::DecalPool pool = gd::make_pool(4);
  const double up[3] = {0, 1, 0};
  const r::Blast b = blast(0, 0, 0, up, 1.0);
  CHECK(r::press_scorch(pool, rec, b) >= 0);
  r::Scorch sc;
  CHECK(r::scorch_of(b, sc));
  std::vector<r::BurnVertex> fresh, cold;
  r::mark_burns(pool, 1.05, {}, fresh);
  r::mark_burns(pool, 30.0, {}, cold);
  CHECK(!fresh.empty() && fresh.size() % 3 == 0 && fresh.size() == cold.size());
  const float u0 = static_cast<float>(sc.variant) / r::kMarkVariants;
  for (const r::BurnVertex& v : fresh) {
    CHECK(v.u >= u0 - 1e-6f && v.u <= u0 + 1.0f / r::kMarkVariants + 1e-6f);
    CHECK(v.v >= -1e-6f && v.v <= 1.0f / gd::kDecalKinds + 1e-6f);  // the scorch's row
    CHECK(v.relief == 0 && v.light[0] == 1);
  }
  CHECK(fresh[0].ember > 0.5f * r::kHotEmberCd && fresh[0].ember <= r::kHotEmberCd * 1.001f);
  CHECK(cold[0].ember == 0 && cold[0].soot > 0);  // the embers out, the soot left
}

TEST(the_atlas_and_the_pass_settings_fit_the_marks) {
  const std::vector<uint8_t>& atlas = r::mark_atlas();
  CHECK(atlas.size() ==
        size_t{r::kMarkAtlasPx} * r::kMarkVariants * r::kMarkAtlasPx * gd::kDecalKinds * 4);
  const r::BeamSettings s = r::mark_beam_settings(10);
  CHECK(s.atlas == &atlas && s.atlas_width == uint32_t{r::kMarkAtlasPx} * r::kMarkVariants);
  CHECK(s.atlas_height == uint32_t{r::kMarkAtlasPx} * gd::kDecalKinds);
  CHECK(s.max_burn_verts == 2400u);
}
