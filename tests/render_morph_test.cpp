// Morph meshes (engine/render/morph.hpp), their CPU twins: two frames blend linearly with the
// normals renormalised and frames past the last clamped, a tinted texel takes its instance's
// ramp at its step in either region while a plain one keeps its sRGB colour decoded and a
// glowing one says so, a ramp runs from light to dark through its colour, and the atlas packs
// skins without overlap or fails when they cannot fit.
//
// ge:summary Tests for morph meshes' CPU twins: frame blending, tinted texels, ramps, the atlas
// ge:keywords test morph pose_morph morph_texel tint ramp ramp_from atlas pack_morph_atlas glow
// ge:skill render testing
#include <vector>

#include "check.hpp"
#include "render/morph.hpp"

namespace {

using namespace ge::render;

MorphModel two_frames() {
  MorphModel m;
  m.positions = {{0, 0, 0, 1, 0, 0}, {2, 0, 0, 1, 4, 0}};
  m.normals = {{1, 0, 0, 0, 1, 0}, {0, 1, 0, 0, 1, 0}};
  m.uv = {0, 0, 1, 1};
  return m;
}

}  // namespace

TEST(two_frames_blend_and_the_normals_stay_unit) {
  const MorphModel m = two_frames();
  std::vector<float> p, n;
  pose_morph(m, 0, 1, 0.5f, p, n);
  CHECK(p[0] == 1 && p[3] == 1 && p[4] == 2);
  CHECK(n[0] > 0.7071f && n[0] < 0.7072f && n[1] > 0.7071f && n[1] < 0.7072f);
  CHECK(n[4] == 1);
  pose_morph(m, 0, 9, 1.0f, p, n);  // a frame past the last is the last
  CHECK(p[0] == 2 && p[4] == 4);
}

TEST(a_tinted_texel_takes_its_ramp_and_a_plain_one_its_colour) {
  TintRamps ramps;
  const float red[3] = {0.6f, 0.05f, 0.04f}, blue[3] = {0.05f, 0.1f, 0.7f};
  CHECK(ramp_from(ramps, red) == 0 && ramp_from(ramps, blue) == 1 && ramps.rows() == 2);
  MorphSkin skin;
  skin.width = 4;
  skin.height = 1;
  skin.rgba = {255, 255, 255, 255, 0, 0, 0, 255, 188, 188, 188, 255, 0, 0, 0, 255};
  skin.marks = {kMarkPlain, static_cast<uint8_t>(kMarkTintA | 7), kMarkGlow,
                static_cast<uint8_t>(kMarkTintB | 15)};
  const uint32_t tint[2] = {1, 0};  // region A blue, B red
  float rgb[3], glow = -1;
  morph_texel(skin, ramps, tint, 0, 0, rgb, &glow);
  CHECK(rgb[0] == 1 && rgb[1] == 1 && rgb[2] == 1 && glow == 0);
  morph_texel(skin, ramps, tint, 1, 0, rgb, &glow);
  const float* step7 = &ramps.rgb[(size_t{1} * kTintSteps + 7) * 3];
  CHECK(rgb[0] == step7[0] && rgb[1] == step7[1] && rgb[2] == step7[2] && glow == 0);
  CHECK(rgb[2] > rgb[0]);  // blue
  morph_texel(skin, ramps, tint, 2, 0, rgb, &glow);
  CHECK(glow == 1 && rgb[0] > 0.49f && rgb[0] < 0.51f);  // sRGB 188 is about half in linear
  morph_texel(skin, ramps, tint, 3, 0, rgb, &glow);
  CHECK(rgb[0] == ramps.rgb[size_t{15} * 3] && rgb[0] > rgb[2]);  // red's darkest
}

TEST(a_ramp_runs_from_light_to_dark_through_its_colour) {
  TintRamps ramps;
  const float green[3] = {0.1f, 0.5f, 0.1f};
  ramp_from(ramps, green);
  float last = 2;
  for (size_t s = 0; s < kTintSteps; ++s) {
    const float g = ramps.rgb[s * 3 + 1];
    CHECK_MSG(g < last, "step " + std::to_string(s) + " is no darker than the one before");
    CHECK(ramps.rgb[s * 3 + 1] > ramps.rgb[s * 3] && ramps.rgb[s * 3 + 1] <= 1);
    last = g;
  }
  CHECK(ramps.rgb[1] > 0.5f && ramps.rgb[(size_t{kTintSteps} - 1) * 3 + 1] < 0.05f);
}

TEST(the_atlas_packs_without_overlap_and_refuses_what_cannot_fit) {
  std::vector<MorphModel> models(40);
  for (size_t i = 0; i < models.size(); ++i) {
    models[i].skin.width = 64 + static_cast<uint32_t>(i * 37 % 200);
    models[i].skin.height = 40 + static_cast<uint32_t>(i * 53 % 160);
  }
  std::vector<AtlasRect> r;
  CHECK(pack_morph_atlas(models, r) && r.size() == models.size());
  for (size_t a = 0; a < r.size(); ++a) {
    CHECK(r[a].w == models[a].skin.width && r[a].h == models[a].skin.height);
    CHECK(r[a].x + r[a].w <= kMorphAtlasSize && r[a].y + r[a].h <= kMorphAtlasSize);
    for (size_t b = a + 1; b < r.size(); ++b) {
      const bool apart = r[a].x + r[a].w <= r[b].x || r[b].x + r[b].w <= r[a].x ||
                         r[a].y + r[a].h <= r[b].y || r[b].y + r[b].h <= r[a].y;
      CHECK_MSG(apart, "skins " + std::to_string(a) + " and " + std::to_string(b) + " overlap");
    }
  }
  std::vector<MorphModel> huge(5);
  for (MorphModel& m : huge) m.skin.width = m.skin.height = 1200;
  CHECK(!pack_morph_atlas(huge, r));
}
