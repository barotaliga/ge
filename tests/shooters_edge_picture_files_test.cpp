// games/shooters_edge/client/picture_files: a picture read by its extension (TGA, PNG, PCX through
// its own palette or the game's, a see-through index); the crosshair picture found under
// crosshairs/ by its bare name or with an extension, the first type in order winning, a name that
// climbs out of the folder or names nothing refused; crosshairimage drawn through the reader in
// place of the mark, even under crosshair 0, at its own pixels times crosshairsize.
//
// ge:summary Tests pictures by file type and the crosshair picture under crosshairs/
// ge:keywords test decode_picture read_crosshair_image crosshairimage tga pcx crosshairs-folder
// ge:skill ui testing
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "assets/pcx.hpp"
#include "assets/tga.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "hud/crosshair.hpp"
#include "shooters_edge/client/picture_files.hpp"

namespace cl = shooters_edge::client;
namespace fs = std::filesystem;

namespace {

void write(const fs::path& p, const std::vector<uint8_t>& bytes) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
}

ge::assets::Image red_square(int n) {
  ge::assets::Image img{n, n, {}};
  for (int i = 0; i < n * n; ++i) img.rgba.insert(img.rgba.end(), {255, 0, 0, 255});
  return img;
}

}  // namespace

TEST(a_picture_reads_by_its_extension) {
  const std::vector<uint8_t> tga = ge::assets::encode_tga(red_square(3));
  std::string why;
  auto img = cl::decode_picture(tga, "crosshairs/a.TGA", {}, &why);
  CHECK_MSG(img && img->width == 3 && img->rgba[0] == 255 && img->rgba[1] == 0, why);
  // A PCX with no palette of its own takes the game's; index 255 see-through when asked.
  ge::assets::IndexedImage ix{2, 1, {7, 255}, {}};
  std::vector<uint8_t> palette(768, 0);
  palette[7 * 3 + 1] = 200;
  img = cl::decode_picture(ge::assets::encode_pcx(ix), "x.pcx", palette, &why, 255);
  CHECK_MSG(img && img->rgba[1] == 200 && img->rgba[3] == 255 && img->rgba[7] == 0, why);
  CHECK(!cl::decode_picture(tga, "x.png", {}, &why) && !why.empty());  // not a PNG
}

TEST(the_crosshair_picture_is_found_under_crosshairs) {
  const fs::path game = fs::temp_directory_path() / "ge_se_picture_files";
  fs::remove_all(game);
  write(game / "crosshairs" / "dot.png", {1, 2, 3});  // not a picture: the TGA wins anyway
  write(game / "crosshairs" / "dot.tga", ge::assets::encode_tga(red_square(5)));
  cl::SearchPath files;
  files.folder = game.string();
  ge::ui::Image out;
  CHECK(cl::read_crosshair_image(files, "dot", &out) && out.width == 5);
  out = {};
  CHECK(cl::read_crosshair_image(files, "dot.pcx", &out) && out.width == 5);  // the type dropped
  CHECK(!cl::read_crosshair_image(files, "../crosshairs/dot", &out));
  CHECK(!cl::read_crosshair_image(files, "none", &out));
  fs::remove_all(game);
}

TEST(crosshairimage_draws_the_players_picture_in_place_of_the_mark) {
  ge::console::Console c;
  ge::hud::CrosshairSettings s;
  ge::hud::add_crosshair_cvars(c.cvars(), s);
  int reads = 0;
  s.read_image = [&](std::string_view name, ge::ui::Image* out) {
    ++reads;
    if (name != "ring") return false;
    out->width = out->height = 8;
    out->rgba.assign(size_t{256}, 255);  // 8 by 8, four bytes a pixel
    return true;
  };
  const ge::ui::Image own;  // a game with no mark of its own
  bool shown = false;
  c.execute("crosshair 0; crosshairimage ring; crosshairsize 2");
  ge::hud::Picture p = ge::hud::crosshair_picture(s, own, 1, {}, &shown);
  CHECK(shown && p.image == &s.picture && p.image->width == 8 && p.density == 0.5f);
  ge::hud::crosshair_picture(s, own, 1, {}, &shown);
  CHECK(reads == 1);  // read once a name
  c.execute("crosshairimage gone");
  p = ge::hud::crosshair_picture(s, own, 1, {}, &shown);
  CHECK(!shown && reads == 2);  // crosshair 0 and no picture: nothing
  c.execute("crosshair 3");
  p = ge::hud::crosshair_picture(s, own, 1, {}, &shown);
  CHECK(shown && p.image == &ge::hud::crosshair_mark(3) && reads == 2);
  const uint64_t revision = s.picture.revision;
  c.execute("crosshairimage ring");
  ge::hud::crosshair_picture(s, own, 1, {}, &shown);
  CHECK(s.picture.revision > revision);  // the GPU's copy uploaded again
}
