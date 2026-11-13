// Morph meshes (engine/render/morph.hpp), their CPU twins: two frames blend linearly with the
// normals renormalised and frames past the last clamped, a tinted texel takes its instance's
// ramp at its step in either region while a plain one keeps its sRGB colour decoded and a
// glowing one says so, a ramp runs from light to dark through its colour, and the atlas packs
// skins without overlap or fails when they cannot fit; a warp at its whole share leaves the draw
// alone, gone it draws nothing and pinches the figure toward its upright line, and as its share
// rises it only ever adds fragments, the head before the feet; along another axis its near end
// goes first, and with no pinch or shimmer no point moves.
//
// ge:summary Tests for morph meshes' CPU twins: frame blending, tinted texels, ramps, the atlas
// ge:keywords test morph pose_morph morph_texel tint ramp ramp_from atlas pack_morph_atlas glow
// ge:keywords warp warp_vertex warp_cut dissolve
// ge:skill render testing
#include <cmath>
#include <string>
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

TEST(a_slot_holds_its_skin_stretched_by_the_nearest_texel_with_the_marks) {
  MorphSkin skin;
  skin.width = 2;
  skin.height = 1;
  skin.rgba = {10, 20, 30, 255, 40, 50, 60, 255};
  skin.marks = {kMarkPlain, 7};
  const std::vector<uint8_t> t = slot_texels(skin);
  CHECK(t.size() == size_t{kMorphSlotSize} * kMorphSlotSize * 4);
  CHECK(t[0] == 10 && t[2] == 30 && t[3] == kMarkPlain);
  const size_t right = (size_t{kMorphSlotSize} * (kMorphSlotSize - 1) + kMorphSlotSize - 1) * 4;
  CHECK(t[right] == 40 && t[right + 3] == 7);
  const size_t mid = size_t{kMorphSlotSize / 2} * 4;  // the second texel starts half way
  CHECK(t[mid] == 40 && t[mid - 4] == 10);
  CHECK(slot_texels(MorphSkin{})[0] == 0);  // an empty skin: a clear slot
}

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

TEST(a_whole_warp_leaves_every_point_and_draws_every_fragment) {
  MorphWarp w;  // shown 1
  const float v[3] = {0.2f, 0.7f, -0.1f};
  float o[3];
  warp_vertex(w, 3.5f, v, o);
  for (int k = 0; k < 3; ++k) CHECK_BITS(o[k], v[k]);
  float edge = -1;
  CHECK(warp_cut(w, v, 0.7f, &edge) && edge == 0);
}

TEST(a_gone_warp_draws_nothing_and_pinches_toward_the_upright_line) {
  MorphWarp w;
  w.shown = 0;
  w.pinch = 1;
  w.shimmer = 1;
  w.low = -0.75f;
  w.high = 1.0f;
  for (int i = 0; i < 400; ++i) {
    const float p[3] = {0.01f * float(i % 37) - 0.2f, 0.004f * float(i) - 0.75f,
                        0.013f * float(i % 11) - 0.07f};
    float edge = 0;
    CHECK_MSG(!warp_cut(w, p, p[1], &edge), "fragment " + std::to_string(i) + " drawn at 0");
  }
  const float v[3] = {0.3f, 0.5f, -0.3f};
  float o[3];
  warp_vertex(w, 0, v, o);
  // Pinched to a fifth of its reach (the shimmer adds at most 6 cm), stretched up from the feet.
  CHECK(std::abs(o[0]) < 0.3f * 0.2f + 0.061f && std::abs(o[2]) < 0.3f * 0.2f + 0.061f);
  CHECK(o[1] > v[1]);
}

TEST(more_shown_draws_everything_less_shown_did_and_the_head_comes_first) {
  MorphWarp w;
  w.low = -0.75f;
  w.high = 1.0f;
  w.seed = 3.7f;
  int head[5] = {}, feet[5] = {};
  const float shares[5] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
  for (int i = 0; i < 2000; ++i) {
    const float p[3] = {0.0071f * float(i % 53) - 0.19f, -0.75f + 1.75f * float(i % 97) / 96.0f,
                        0.0093f * float(i % 29) - 0.13f};
    bool was = false;
    for (int s = 0; s < 5; ++s) {
      w.shown = shares[s];
      float edge = 0;
      const bool drawn = warp_cut(w, p, p[1], &edge);
      CHECK_MSG(!was || drawn, "point " + std::to_string(i) + " lost as the share rose");
      CHECK(edge >= 0 && edge <= 1);
      was = drawn;
      if (drawn && p[1] > 0.6f) head[s]++;
      if (drawn && p[1] < -0.4f) feet[s]++;
    }
  }
  CHECK_MSG(head[2] > feet[2], "half shown, the head should lead: head " + std::to_string(head[2]) +
                                   " feet " + std::to_string(feet[2]));
}

TEST(the_near_end_of_the_axis_goes_first_and_without_pinch_the_points_stay) {
  // Swept along +x: the -x side is the near end, under the cut last as the share falls.
  MorphWarp w;
  w.axis[0] = 1;
  w.axis[1] = 0;
  w.low = -0.3f;
  w.high = 0.3f;
  double near = 0, far = 0;
  for (int i = 0; i < 500; ++i) {
    const float y = 0.004f * float(i) - 1, z = 0.0007f * float(i % 41);
    const float a[3] = {-0.25f, y, z}, b[3] = {0.25f, y, z};
    near += warp_field(w, a, a[0]);
    far += warp_field(w, b, b[0]);
  }
  CHECK_MSG(near > far, "the near end's field should stand above the far end's");
  w.shown = 0.3f;
  const float v[3] = {0.2f, 0.4f, -0.1f};
  float o[3];
  warp_vertex(w, 1.25f, v, o);
  for (int k = 0; k < 3; ++k) CHECK_BITS(o[k], v[k]);
}
