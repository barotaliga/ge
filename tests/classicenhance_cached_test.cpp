// The cache read back by a game (plugins/classicenhance/cached.hpp): a texture's enhanced copy
// at a multiple of the pack's size, its glowing texels marked in alpha; a size that fits no
// multiple, a liquid's or the sky's name, and a missing file all leave the pack's own; a model's
// skins in order; and a pack whose textures were never enhanced has no cache.
//
// ge:summary Tests reading the enhanced cache back: textures, glow in alpha, skins, refusals
// ge:keywords test classicenhance cache cached_texture cached_skins find_cache glow alpha
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

  picture(dir / "models" / "soldier" / "skin0.png", 64, 32, 10);
  picture(dir / "models" / "soldier" / "skin1.png", 64, 32, 20);
  std::vector<ge::assets::Image> skins;
  CHECK(ce::cached_skins(dir, "soldier", 16, 8, &skins));
  CHECK(skins.size() == 2 && skins[1].width == 64 && skins[1].rgba[0] == 20);
  CHECK(!ce::cached_skins(dir, "soldier", 15, 8, &skins));
  CHECK(!ce::cached_skins(dir, "dog", 16, 8, &skins));
  fs::remove_all(root);
}
