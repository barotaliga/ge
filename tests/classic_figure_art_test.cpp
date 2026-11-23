// The classic figure art (plugins/classic/figure_art.hpp): a player's colours put on a skin, a
// skin picture of indices worn over the model's, two meshes joined into one picture, and a frame
// drawn on the CPU on the turntable.
//
// ge:summary Tests the classic figure art: colours, a worn skin picture, joined meshes, draw_turned
// ge:keywords figure-art translate wear_indexed_skin join_meshes draw_turned turntable
// ge:skill classic morph
#include <string>
#include <vector>

#include "check.hpp"
#include "classic/figure_art.hpp"
#include "morph/write.hpp"
#include "overview/mesh_picture.hpp"

namespace gc = ge::classic;

namespace {

gc::ModelArt sample_art() {
  gc::ModelArt a;
  a.model = ge::morph::sample_model();
  a.model.skin_width = 8;
  a.model.skin_height = 4;
  a.palette.resize(768);
  for (size_t i = 0; i < 256; ++i) {
    a.palette[i * 3] = static_cast<uint8_t>(i);
    a.palette[i * 3 + 1] = static_cast<uint8_t>(255 - i);
    a.palette[i * 3 + 2] = 7;
  }
  a.ok = true;
  return a;
}

}  // namespace

TEST(colours_go_on_the_shirt_and_pants_rows_and_a_bright_row_runs_backwards) {
  CHECK(gc::translate(16, 4, 12) == 64);
  CHECK(gc::translate(31, 4, 12) == 79);
  CHECK(gc::translate(96, 4, 12) == 207);  // row 12 is stored bright to dark: read backwards
  CHECK(gc::translate(111, 4, 12) == 192);
  CHECK(gc::translate(50, 4, 12) == 50);            // neither range: the skin's own
  CHECK(gc::translate(16, -1, 12) == 16);           // -1 keeps the shirt's own
  CHECK(gc::translate(16, 99, 0) == 13 * 16 + 15);  // a colour past 13 is 13
}

TEST(a_skin_picture_of_indices_is_worn_over_the_whole_skin) {
  gc::ModelArt a = sample_art();
  // A field-sized picture: the worn part is its top-left 296 x 194, stretched over the 8 x 4 skin.
  std::vector<uint8_t> pic(320 * 200, 0);
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 320; ++x) pic[size_t(y) * 320 + size_t(x)] = x < 148 ? 16 : 96;
  CHECK(gc::wear_indexed_skin(a, pic, 320, 200));
  CHECK(a.model.skins.size() == 3);
  CHECK(a.model.skins[0].indices[0] == 16 && a.model.skins[0].indices[7] == 96);
  CHECK(a.model.skins[2].group == 1);  // the groups and intervals kept
  // A smaller picture sits at the field's top left, index 0 past it.
  gc::ModelArt b = sample_art();
  CHECK(gc::wear_indexed_skin(b, std::vector<uint8_t>(10 * 10, 50), 10, 10));
  CHECK(b.model.skins[0].indices[0] == 50 && b.model.skins[0].indices[7] == 0);
  CHECK(!gc::wear_indexed_skin(b, std::vector<uint8_t>(400 * 10, 1), 400, 10));  // too wide
}

TEST(two_meshes_joined_draw_as_one_with_their_own_textures) {
  const gc::ModelArt a = sample_art();
  ge::overview::Mesh m = gc::art_mesh(a, 0, 0, 4, 12);
  const ge::overview::Mesh add = gc::art_mesh(a, 0, 0, -1, -1);
  const size_t verts = m.pos.size() / 3, tris = m.idx.size();
  gc::join_meshes(m, add);
  CHECK(m.pos.size() / 3 == verts * 2 && m.idx.size() == tris * 2);
  CHECK(m.th == 8 && m.tw == 8);
  CHECK(m.rect[verts * 4 + 1] == 4.0f);  // the second's texture under the first's
  CHECK(m.idx[tris] == add.idx[0] + verts);
  ge::overview::TurnView v;
  v.width = 48;
  v.height = 64;
  v.px_per_unit = 1;
  const ge::ui::Image img = ge::overview::draw_turned(m, v);
  CHECK(img.width == 48 && img.height == 64);
  CHECK(img.rgba[(size_t(32) * 48 + 24) * 4 + 3] == 255);  // the middle covered
  CHECK(img.rgba[3] == 0);                                 // a corner clear, and no rim
}
