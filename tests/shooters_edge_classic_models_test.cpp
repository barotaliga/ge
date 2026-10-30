// The classic models as the engine draws them (games/shooters_edge/app/classic_models.hpp): a
// model's skin keeps the palette's colours and marks its glowing and tinted texels; its fronts
// face out once the file's frame is reflected into the engine's; its frames and skins play by
// group; an entity's yaw turns it the way the map turns it; a sprite's origin lands where the file
// puts it and its clear index is clear; the light under a point is read off the floor's lightmap.
//
// ge:summary Tests the classic models' conversion: skins, marks, winding, groups, yaw, sprites, light
// ge:keywords test classic-models morph_from_alias sprite_frames_of frame_in rotation_of light_under
// ge:skill shooter morph
#include <cmath>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "morph/write.hpp"
#include "shooters_edge/app/classic_models.hpp"

namespace {

namespace md = shooters_edge::models;
namespace gr = ge::render;

std::vector<uint8_t> grey_palette() {
  std::vector<uint8_t> p(768);
  for (size_t i = 0; i < 768; i++) p[i] = uint8_t(i / 3);
  return p;
}

// `v` turned by the unit quaternion `q` (x, y, z, w).
void turn(const float q[4], const double v[3], double out[3]) {
  const double x = q[0], y = q[1], z = q[2], w = q[3];
  const double t[3] = {2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]),
                       2 * (x * v[1] - y * v[0])};
  out[0] = v[0] + w * t[0] + (y * t[2] - z * t[1]);
  out[1] = v[1] + w * t[1] + (z * t[0] - x * t[2]);
  out[2] = v[2] + w * t[2] + (x * t[1] - y * t[0]);
}

}  // namespace

TEST(a_skin_keeps_the_palettes_colours_and_marks_glow_and_tints) {
  ge::morph::Model m = ge::morph::sample_model();
  ge::morph::Skin& s = m.skins[0];
  s.indices[0] = 5;     // plain
  s.indices[1] = 17;    // the shirt's row, step 1
  s.indices[2] = 100;   // the pants' row, step 4
  s.indices[3] = 230;   // fullbright
  const gr::MorphModel out = md::morph_from_alias(m, grey_palette(), 0);
  CHECK(out.skin.width == uint32_t(s.width) && out.skin.height == uint32_t(s.height));
  CHECK(out.skin.rgba[0] == 5 && out.skin.rgba[1] == 5 && out.skin.rgba[2] == 5);
  CHECK(out.skin.marks[0] == gr::kMarkPlain);
  CHECK(out.skin.marks[1] == (gr::kMarkTintA | 1));
  CHECK(out.skin.marks[2] == (gr::kMarkTintB | 4));
  CHECK(out.skin.marks[3] == gr::kMarkGlow);
  // A skin out of range is the last one; every frame came across.
  CHECK(md::morph_from_alias(m, grey_palette(), 99).skin.rgba ==
        md::morph_from_alias(m, grey_palette(), int(m.skins.size()) - 1).skin.rgba);
  CHECK(out.positions.size() == m.frames.size());
}

TEST(a_tinted_texel_drawn_in_its_own_row_is_its_own_colour) {
  const gr::TintRamps r = md::palette_ramps(grey_palette());
  CHECK(r.rows() == md::kColourRows);
  // Row 6, step 4 is index 100: grey 100 in linear light.
  const double v = 100 / 255.0, lin = std::pow((v + 0.055) / 1.055, 2.4);
  const size_t at = (size_t(md::kPantsRow) * gr::kTintSteps + 4) * 3;
  CHECK(std::abs(r.rgb[at] - lin) < 1e-6);
}

TEST(the_fronts_face_out_in_the_engines_frame) {
  const gr::MorphModel out = md::morph_from_alias(ge::morph::sample_model(), grey_palette(), 0);
  const std::vector<float>& p = out.positions[0];
  int outward = 0, triangles = 0;
  for (size_t t = 0; t + 2 < out.indices.size(); t += 3, triangles++) {
    const float* a = &p[out.indices[t] * 3];
    const float* b = &p[out.indices[t + 1] * 3];
    const float* c = &p[out.indices[t + 2] * 3];
    const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                         e1[0] * e2[1] - e1[1] * e2[0]};
    const double mid = n[0] * (a[0] + b[0] + c[0]) + n[1] * (a[1] + b[1] + c[1]) +
                       n[2] * (a[2] + b[2] + c[2]);
    if (mid > 0) outward++;
  }
  CHECK(triangles == 8);
  CHECK(outward == triangles);
}

TEST(a_group_plays_its_frames_at_their_intervals_and_loops) {
  const ge::morph::Model m = ge::morph::sample_model();
  const md::Groups g = md::frame_groups(m);
  CHECK(g.first.size() == 2);
  CHECK(md::frame_in(g, 0, 7.3) == 0);   // a lone frame
  CHECK(md::frame_in(g, 1, 0.05) == 1);  // the group's first, for 0.1 s
  CHECK(md::frame_in(g, 1, 0.15) == 2);
  CHECK(md::frame_in(g, 1, 0.25) == 1);  // looped
  CHECK(md::frame_in(g, 9, 0.15) == 0);  // out of range: group 0
  const md::Groups s = md::skin_groups(m);
  CHECK(md::frame_in(s, 1, 0.3) == 2);  // the second skin of the second group
}

TEST(a_yaw_turns_the_model_as_the_map_turns_it) {
  // Yaw 90 turns the model's forward (the file's +x, the engine's +x) to the file's +y, which is
  // the engine's +z.
  const float angles[3] = {0, 90, 0};
  float q[4];
  md::rotation_of(angles, true, q);
  const double fwd[3] = {1, 0, 0};
  double out[3];
  turn(q, fwd, out);
  CHECK(std::abs(out[0]) < 1e-6 && std::abs(out[1]) < 1e-6 && std::abs(out[2] - 1) < 1e-6);
}

TEST(a_sprites_origin_and_clear_index_land_where_the_file_puts_them) {
  ge::morph::Sprite s = ge::morph::sample_sprite();
  s.frames[0].indices[0] = 255;
  const std::vector<gr::SpriteFrame> f = md::sprite_frames_of(s, grey_palette());
  CHECK(f.size() == 3);
  CHECK(f[0].width == 8 && f[0].height == 6);
  CHECK(f[0].origin[0] == 4 && f[0].origin[1] == 3);  // the middle
  CHECK(f[0].rgba[3] == 0);                           // clear
  CHECK(f[0].rgba[7] == 255);
  CHECK(md::frame_in(md::sprite_groups(s), 1, 0.15) == 2);
}

TEST(the_light_under_a_point_is_the_floors_lightmap) {
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error));
  ge::brush::Map map;
  CHECK(ge::brush::read_bsp(bsp, &map, &error));
  const md::LightProbe probe = md::make_probe(map);
  CHECK(!probe.tris.empty());
  float rgb[3] = {0, 0, 0};
  const float over_floor[3] = {128, 192, 40};
  CHECK(md::light_under(probe, map, over_floor, rgb));
  CHECK(rgb[0] > 0 && rgb[1] > 0 && rgb[2] > 0);
  const float far_above[3] = {128, 192, 5000};
  CHECK(!md::light_under(probe, map, far_above, rgb));
}
