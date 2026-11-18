// The overview (engine/overview) and its readers' halves: a level's floors from above with a
// bridge over a floor and a pool beside it (what is under the bridge reads as covered, what is
// under the water as tinted and the water as glints), the picture fitted to an area, a model's
// group of frames played on its intervals and one frame drawn as a figure in a remapped skin.
//
// ge:summary Tests: the overview's picture (roofs, liquids, glints, fit) and a model as a figure
// ge:keywords overview test covered liquid-tint water-layer frame-group overview_mesh draw_figure
// ge:skill map-design testing
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "morph/overview_mesh.hpp"
#include "overview/mesh_picture.hpp"
#include "overview/plate.hpp"

namespace ov = ge::overview;
namespace mo = ge::morph;

namespace {

// A level 512 units square: a floor at 0, a bridge at 128 across x in 192..320, a pool of water
// (surface at -8, floor at -64) where x > 384.
ov::Picture sample() {
  const double lo[3] = {0, 0, -64}, hi[3] = {512, 512, 128};
  const ov::Frame fr = ov::frame_for(lo, hi, 300);
  ov::Floors f = ov::empty_floors(fr, 2);
  for (int Y = 0; Y < f.h; ++Y)
    for (int X = 0; X < f.w; ++X) {
      const double x = fr.lo[0] + (X / double(f.ss) - fr.pad) / fr.scale;
      const double y = fr.hi[1] - (Y / double(f.ss) - fr.pad) / fr.scale;
      if (x < 0 || x > 512 || y < 0 || y > 512) continue;
      const size_t at = size_t(Y) * size_t(f.w) + size_t(X);
      const bool bridge = x >= 192 && x <= 320;
      const bool pool = x > 384;
      f.top[at] = bridge ? 128.0f : pool ? -64.0f : 0.0f;
      for (int q = 0; q < 3; ++q) f.rgb[at * 3 + size_t(q)] = 0.5f;
      if (pool) {
        f.wet[at] = -8;
        f.wrgb[at * 3 + 2] = 0.8f;
        f.wkind[at] = ov::kWater;
      }
    }
  ov::Picture p;
  ov::picture_from_floors(f, fr, &p);
  return p;
}

}  // namespace

TEST(overview_reads_what_lies_under_a_bridge_as_covered) {
  const ov::Picture p = sample();
  CHECK(p.image.width <= 300 && p.image.height <= 300);
  CHECK(p.covered(256, 256, 0, 48));     // under the bridge
  CHECK(!p.covered(256, 256, 130, 48));  // on it
  CHECK(!p.covered(100, 256, 0, 48));    // in the open
  CHECK(!p.covered(-500, 0, 0, 48));     // off the picture
}

TEST(overview_keeps_the_liquid_as_a_layer_over_what_is_under_it) {
  const ov::Picture p = sample();
  CHECK(p.liquid_surface(450, 256) == -8.0f);
  CHECK(p.liquid_surface(100, 256) == ov::kNone);
  const ge::ui::Color dry = p.liquid_tint(100, 256, 0);
  CHECK(dry.r == 1 && dry.a == 1);
  const ge::ui::Color wet = p.liquid_tint(450, 256, -40);
  CHECK(wet.a < 1);         // seen through the water
  CHECK(wet.b > wet.r);     // tinted by its colour
  CHECK(!p.water.empty());  // the glints play on the pool
  for (uint8_t k : p.water_kind) CHECK(k == ov::kWater);
  CHECK(p.liquid.image.width == p.image.width);
}

TEST(overview_fits_the_picture_in_an_area_centred) {
  const ov::Picture p = sample();
  const ge::ui::Rect r = ov::fit_rect(p, {0, 0, 800, 200});
  CHECK(r.h == 200);
  CHECK(r.x > 0);
  CHECK(ov::fit_rect(ov::Picture{}, {10, 20, 100, 100}).w == 0);
}

TEST(overview_plays_a_models_frames_and_draws_one_as_a_figure) {
  mo::Model m;
  m.st = {0, 0, 1, 0, 0, 1};
  m.indices = {0, 1, 2};
  for (int i = 0; i < 3; ++i) {
    mo::Frame f;
    f.positions = {-20, -20, 0, 20, -20, 0, -20, 20, float(4 * i)};
    f.group = i < 2 ? 1 : 2;  // two frames in group 1, one in group 2
    f.interval = i < 2 ? 0.1f : 0;
    m.frames.push_back(f);
  }
  CHECK(mo::frame_at(m, 1, 0.05) == 0);
  CHECK(mo::frame_at(m, 1, 0.15) == 1);
  CHECK(mo::frame_at(m, 1, 0.25) == 0);  // round again
  CHECK(mo::frame_at(m, 2, 9.0) == 2);
  CHECK(mo::frame_at(m, 7, 0.0) == 0);  // a group the model lacks
  mo::Skin s;
  s.width = s.height = 2;
  s.indices = {20, 20, 20, 20};
  m.skins.push_back(s);
  std::vector<uint8_t> pal(768, 0);
  pal[size_t(20) * 3] = 255;      // index 20 red
  pal[size_t(40) * 3 + 2] = 255;  // index 40 blue
  std::vector<uint8_t> remap(256);
  for (size_t i = 0; i < 256; ++i) remap[i] = uint8_t(i == 20 ? 40 : i);
  mo::SkinPaint paint;
  paint.palette = pal;
  paint.remap = remap;
  const ov::Mesh mesh = mo::overview_mesh(m, 2, 0, paint);
  CHECK(mesh.tw == 2 && mesh.tex.size() == 16);
  CHECK(mesh.tex[0] == 0 && mesh.tex[2] == 255);  // painted through the remap
  CHECK(mesh.pos[8] == 8.0f);                     // the third frame's positions
  const ge::ui::Image fig = ov::draw_figure(mesh, 64, 0, 0, 90, 1.0);
  CHECK(fig.width == 64);
  int seen = 0;
  for (size_t i = 3; i < fig.rgba.size(); i += 4) seen += fig.rgba[i] > 0;
  CHECK(seen > 50);
  CHECK(mo::overview_mesh(mo::Model{}, 0, 0, paint).pos.empty());
}
