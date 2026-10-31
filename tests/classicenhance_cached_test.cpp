// The cache read back by a game (plugins/classicenhance/cached.hpp): a texture's enhanced copy
// at a multiple of the pack's size, its glowing texels marked in alpha; a size that fits no
// multiple, a liquid's or the sky's name, and a missing file all leave the pack's own; a model's
// skins in order; its relief (normal, height, roughness) packed at the albedo's size; and a pack
// whose textures were never enhanced has no cache.
//
// ge:summary Tests reading the enhanced cache back: textures, glow in alpha, skins, refusals
// ge:keywords test classicenhance cache cached_texture cached_skins find_cache glow alpha
// ge:keywords cached_surface relief normal height roughness
// ge:skill classicenhance testing
#include <filesystem>
#include <string>
#include <vector>

#include "assets/sha256.hpp"
#include "check.hpp"
#include "classicenhance/cache.hpp"
#include "classicenhance/cached.hpp"
#include "lab/png.hpp"

namespace ce = ge::classicenhance;
namespace fs = std::filesystem;

namespace {

void picture(const fs::path& path, int w, int h, uint8_t v) {
  fs::create_directories(path.parent_path());
  ge::lab::Picture p{w, h, std::vector<uint8_t>(size_t(w) * size_t(h) * 4, v)};
  for (size_t i = 3; i < p.rgba.size(); i += 4) p.rgba[i] = 255;
  CHECK(ge::lab::write_png(path.string(), p));
}

}  // namespace

TEST(textures_and_skins_come_back_at_a_multiple_of_the_packs_size) {
  const fs::path root = fs::temp_directory_path() / "ge_classicenhance_cached";
  fs::remove_all(root);
  const std::vector<uint8_t> pack = {1, 2, 3, 4};
  CHECK(ce::find_cache(pack, root).empty());
  const fs::path dir = ce::pack_dir(root, ge::assets::sha256_hex(pack));
  CHECK(ce::write_record(dir, "textures", "k"));
  CHECK(ce::find_cache(pack, root) == dir);

  picture(dir / "textures" / "wall1" / "albedo.png", 32, 16, 100);
  picture(dir / "textures" / "lamp" / "albedo.png", 16, 16, 90);
  {
    // The lamp's glow: its left half lit.
    ge::lab::Picture g{16, 16, std::vector<uint8_t>(size_t{16} * 16 * 4, 0)};
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 8; x++) g.rgba[(size_t(y) * 16 + size_t(x)) * 4] = 200;
    CHECK(ge::lab::write_png((dir / "textures" / "lamp" / "emissive.png").string(), g));
  }
  std::vector<uint8_t> rgba;
  CHECK(ce::cached_texture(dir, "WALL1", 16, 8, &rgba));  // 2x, by its folder's name
  CHECK(rgba.size() == size_t{32} * 16 * 4 && rgba[0] == 100 && rgba[3] == 0);
  CHECK(ce::cached_texture(dir, "lamp", 4, 4, &rgba));  // 4x
  CHECK(rgba[3] == 255 && rgba[(15 * 4) + 3] == 0);
  rgba.clear();
  CHECK(!ce::cached_texture(dir, "wall1", 10, 8, &rgba) && rgba.empty());  // fits no multiple
  CHECK(!ce::cached_texture(dir, "none", 16, 8, &rgba));
  CHECK(!ce::cached_texture(dir, "*water", 16, 8, &rgba));
  CHECK(!ce::cached_texture(dir, "sky4", 16, 8, &rgba));

  // The relief: the normal's x, y, the height and the roughness in one texel, at the albedo's size.
  CHECK(!ce::cached_surface(dir, "wall1", 32, 16, &rgba));  // no maps yet
  picture(dir / "textures" / "wall1" / "normal.png", 32, 16, 128);
  picture(dir / "textures" / "wall1" / "height.png", 32, 16, 200);
  CHECK(!ce::cached_surface(dir, "wall1", 32, 16, &rgba));  // no roughness
  picture(dir / "textures" / "wall1" / "roughness.png", 32, 16, 190);
  CHECK(ce::cached_surface(dir, "WALL1", 32, 16, &rgba));
  CHECK(rgba.size() == size_t{32} * 16 * 4 && rgba[0] == 128 && rgba[1] == 128 && rgba[2] == 200 &&
        rgba[3] == 190);
  CHECK(!ce::cached_surface(dir, "wall1", 16, 8, &rgba));  // the albedo's size, not the pack's
  CHECK(!ce::cached_surface(dir, "*water", 32, 16, &rgba));

  picture(dir / "models" / "soldier" / "skin0.png", 64, 32, 10);
  picture(dir / "models" / "soldier" / "skin1.png", 64, 32, 20);
  std::vector<ge::assets::Image> skins;
  CHECK(ce::cached_skins(dir, "soldier", 16, 8, &skins));
  CHECK(skins.size() == 2 && skins[1].width == 64 && skins[1].rgba[0] == 20);
  CHECK(!ce::cached_skins(dir, "soldier", 15, 8, &skins));
  CHECK(!ce::cached_skins(dir, "dog", 16, 8, &skins));
  fs::remove_all(root);
}

// A map's rebake comes back only whole and made for a map of the same vertices.
TEST(a_rebake_comes_back_for_its_own_map_only) {
  const fs::path dir = fs::temp_directory_path() / "ge_classicenhance_rebake";
  fs::remove_all(dir);
  ge::brush::Rebaked r;
  r.width = 2;
  r.height = 1;
  r.rgb9e5 = {1, 2};
  r.bounce = {3, 4};
  r.dir.assign(8, 128);
  r.lm = {0.25f, 0.5f, -1, -1, 0.75f, 0.5f};  // three vertices
  ge::brush::Rebaked back;
  CHECK(!ce::cached_rebake(dir, "e1m1", 3, &back));  // none yet
  CHECK(ce::write_whole(dir / "lightmaps" / "e1m1.gerb", ge::brush::write_rebaked(r)));
  CHECK(!ce::cached_rebake(dir, "e1m1", 4, &back));  // another map's
  CHECK(back.rgb9e5.empty());
  CHECK(ce::cached_rebake(dir, "e1m1", 3, &back));
  CHECK(back.bounce == r.bounce && back.lm == r.lm);
  std::vector<uint8_t> cut = ge::brush::write_rebaked(r);
  cut.pop_back();
  CHECK(ce::write_whole(dir / "lightmaps" / "e1m2.gerb", cut));
  CHECK(!ce::cached_rebake(dir, "e1m2", 3, &back));
  fs::remove_all(dir);
}
