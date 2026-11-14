// Tests: what the window draws round the sequel's bar (games/shooters_edge/client/sequel_bar.cpp):
// the bar's whole-number scale, the centre print's lines (centred, cut at 40 letters, placed at
// 35% of the screen or at 48 for a long message) and the crosshair left out when the packs hold
// no picture of it.
//
// ge:summary Tests: the sequel's centre print and crosshair round its bar, and the bar's scale
// ge:keywords test sequel centre-print crosshair status-bar scale add_sequel_screen
// ge:skill ui testing
// ge:test tests/shooters_edge_sequel_screen_test.cpp
#include <filesystem>
#include <string>
#include <vector>

#include "assets/pcx.hpp"
#include "check.hpp"
#include "lab/png.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/sequel_bar.hpp"
#include "vfs/pak.hpp"

namespace {

namespace cl = shooters_edge::client;

std::string placed(const cl::SequelBar& bar) {
  std::string s;
  for (const auto& o : bar.ops)
    s += "'" + o.text + "'@" + std::to_string(o.x) + "," + std::to_string(o.y) + " ";
  return s;
}

}  // namespace

TEST(the_bar_is_drawn_a_whole_number_larger_for_every_400_px_of_height) {
  CHECK(cl::sequel_bar_scale(300) == 1);
  CHECK(cl::sequel_bar_scale(480) == 1);
  CHECK(cl::sequel_bar_scale(800) == 2);
  CHECK(cl::sequel_bar_scale(1440) == 3);
}

TEST(a_centre_print_centres_each_line_at_35_percent_of_the_screen) {
  cl::SequelBar bar;
  cl::add_sequel_screen(bar, 0, "You need the key\nto open it", 640, 480);
  CHECK(placed(bar) == "'You need the key'@256,168 'to open it'@280,176 ");
}

TEST(a_long_centre_print_starts_at_48_and_a_line_is_cut_at_40_letters) {
  cl::SequelBar bar;
  cl::add_sequel_screen(bar, 0, "a\nb\nc\nd\n" + std::string(50, 'x'), 640, 480);
  CHECK(bar.ops.size() == 5);
  CHECK(bar.ops[0].y == 48);
  CHECK(bar.ops[4].y == 80);
  CHECK(bar.ops[4].text.size() == 40);
  CHECK(bar.ops[4].x == 160);
}

TEST(on_a_tall_window_the_centre_print_is_placed_on_the_smaller_screen) {
  cl::SequelBar bar;
  cl::add_sequel_screen(bar, 0, "hi", 1600, 1000);  // drawn twice over: an 800 x 500 screen
  CHECK(placed(bar) == "'hi'@392,175 ");
}

TEST(no_crosshair_is_drawn_when_the_packs_hold_no_picture_of_it) {
  cl::SequelBar bar;  // no files: every picture is missing
  cl::add_sequel_screen(bar, 1, "", 640, 480);
  CHECK(bar.ops.empty());
  cl::add_sequel_screen(bar, 0, "", 640, 480);
  CHECK(bar.ops.empty());
}

TEST(an_enhanced_picture_is_drawn_at_the_packs_size_filtered_from_the_cache) {
  // The crosshair's picture is in the packs' cache at 4x (classic-enhance's pictures stage), the
  // health icon is not: the crosshair is placed and drawn by its pack size from the cache's
  // pixels, filtered; the icon is the pack's own, sharp.
  const auto pcx = [](int w, int h) {
    ge::assets::IndexedImage img;
    img.width = w;
    img.height = h;
    img.indices.assign(size_t(w) * size_t(h), 7);
    img.palette.assign(768, 90);
    return ge::assets::encode_pcx(img);
  };
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak({{"pics/ch1.pcx", pcx(8, 8)}, {"pics/i_health.pcx", pcx(24, 24)}}, &pak,
                           &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  const std::filesystem::path cache =
      std::filesystem::temp_directory_path() / "ge_se_sequel_bar_enhanced";
  std::filesystem::remove_all(cache);
  std::filesystem::create_directories(cache / "pictures" / "enhanced" / "pics");
  ge::lab::Picture big{32, 32, std::vector<uint8_t>(size_t{32} * 32 * 4, 200)};
  CHECK(ge::lab::write_png((cache / "pictures" / "enhanced" / "pics" / "ch1.png").string(), big));
  cl::SequelBar bar;
  bar.files = &files;
  bar.cache = cache;
  cl::add_sequel_screen(bar, 1, "", 640, 480);
  CHECK(bar.ops.size() == 1 && bar.ops[0].x == 316 && bar.ops[0].y == 236);
  shooters_edge::hud::LayoutOp icon;
  icon.name = "i_health";
  bar.ops.push_back(icon);
  ge::ui::DrawList dl;
  dl.begin(640, 480);
  CHECK(cl::draw_sequel_bar(dl, bar, 2));
  CHECK(dl.cmds().size() == 2);
  if (dl.cmds().size() == 2) {
    const ge::ui::Box& ch = dl.cmds()[0].box;
    CHECK(ch.rect.w == 16 && ch.image->width == 32 && !ch.nearest);
    const ge::ui::Box& hp = dl.cmds()[1].box;
    CHECK(hp.rect.w == 48 && hp.image->width == 24 && hp.nearest);
  }
  std::filesystem::remove_all(cache);
}
