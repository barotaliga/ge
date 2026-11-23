// Tests: what the window draws round Quake II's bar (games/shooters_edge/client/q2_bar.cpp):
// the bar's whole-number scale, the centre print's lines (centred, cut at 40 letters, placed at
// 35% of the screen or at 48 for a long message) and the crosshair left out when the packs hold
// no picture of it.
//
// ge:summary Tests: Quake II's centre print and crosshair round its bar, and the bar's scale
// ge:keywords test q2 centre-print crosshair status-bar scale add_q2_screen
// ge:skill ui testing
// ge:test tests/shooters_edge_q2_screen_test.cpp
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/pcx.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "lab/png.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/q2_bar.hpp"
#include "vfs/pak.hpp"

namespace {

namespace cl = shooters_edge::client;

std::string placed(const cl::Q2Bar& bar) {
  std::string s;
  for (const auto& o : bar.ops)
    s += "'" + o.text + "'@" + std::to_string(o.x) + "," + std::to_string(o.y) + " ";
  return s;
}

}  // namespace

TEST(the_2d_scale_is_picked_by_the_window_as_quake_ii_picks_it) {
  CHECK(cl::q2_bar_scale(640, 480) == 1);
  CHECK(cl::q2_bar_scale(1280, 1079) == 1);
  CHECK(cl::q2_bar_scale(1920, 1080) == 2);
  CHECK(cl::q2_bar_scale(2560, 1440) == 2);  // was 3 by 400 px a step
  CHECK(cl::q2_bar_scale(3840, 2160) == 4);
  CHECK(cl::q2_bar_scale(1920, 2400) == 2);  // a tall window: by its width
  CHECK(cl::q2_bar_scale(1080, 1920) == 1);
  CHECK(cl::q2_bar_scale(640, 480, 3) == 3);  // scr_scale as given
  CHECK(cl::q2_bar_scale(640, 480, 40) == 10);
  cl::Q2Screen s;
  s.scale = 2;
  cl::Q2Bar bar;
  CHECK(cl::q2_bar_scale(bar, 640, 480) == 1);
  bar.screen = &s;
  CHECK(cl::q2_bar_scale(bar, 640, 480) == 2);
}

TEST(scr_draw2d_0_draws_no_centre_print_and_1_keeps_it) {
  cl::Q2Screen s;
  cl::Q2Bar bar;
  bar.screen = &s;
  s.draw2d = 0;
  cl::add_q2_screen(bar, 0, "Hidden", 640, 480);
  CHECK(bar.ops.empty());
  s.draw2d = 1;
  cl::add_q2_screen(bar, 0, "Shown", 640, 480);
  CHECK(bar.ops.size() == 1);
}

TEST(a_centre_print_centres_each_line_at_35_percent_of_the_screen) {
  cl::Q2Bar bar;
  cl::add_q2_screen(bar, 0, "You need the key\nto open it", 640, 480);
  CHECK(placed(bar) == "'You need the key'@256,168 'to open it'@280,176 ");
}

TEST(a_long_centre_print_starts_at_48_and_a_line_is_cut_at_40_letters) {
  cl::Q2Bar bar;
  cl::add_q2_screen(bar, 0, "a\nb\nc\nd\n" + std::string(50, 'x'), 640, 480);
  CHECK(bar.ops.size() == 5);
  CHECK(bar.ops[0].y == 48);
  CHECK(bar.ops[4].y == 80);
  CHECK(bar.ops[4].text.size() == 40);
  CHECK(bar.ops[4].x == 160);
}

TEST(on_a_large_window_the_centre_print_is_placed_on_the_smaller_screen) {
  cl::Q2Bar bar;
  cl::add_q2_screen(bar, 0, "hi", 1920, 1200);  // drawn twice over: a 960 x 600 screen
  CHECK(placed(bar) == "'hi'@472,210 ");
}

TEST(no_crosshair_is_drawn_when_the_packs_hold_no_picture_of_it) {
  cl::Q2Bar bar;  // no files: every picture is missing
  cl::add_q2_screen(bar, 1, "", 640, 480);
  CHECK(bar.ops.empty());
  cl::add_q2_screen(bar, 0, "", 640, 480);
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
      std::filesystem::temp_directory_path() / "ge_se_q2_bar_enhanced";
  std::filesystem::remove_all(cache);
  std::filesystem::create_directories(cache / "pictures" / "enhanced" / "pics");
  ge::lab::Picture big{32, 32, std::vector<uint8_t>(size_t{32} * 32 * 4, 200)};
  CHECK(ge::lab::write_png((cache / "pictures" / "enhanced" / "pics" / "ch1.png").string(), big));
  cl::Q2Bar bar;
  bar.files = &files;
  bar.cache = cache;
  cl::add_q2_screen(bar, 1, "", 640, 480);
  CHECK(bar.ops.size() == 1 && bar.ops[0].x == 316 && bar.ops[0].y == 236);
  shooters_edge::hud::LayoutOp icon;
  icon.name = "i_health";
  bar.ops.push_back(icon);
  ge::ui::DrawList dl;
  dl.begin(640, 480);
  CHECK(cl::draw_q2_bar(dl, bar, 2));
  CHECK(dl.cmds().size() == 2);
  if (dl.cmds().size() == 2) {
    const ge::ui::Box& ch = dl.cmds()[0].box;
    CHECK(ch.rect.w == 16 && ch.image->width == 32 && !ch.nearest);
    const ge::ui::Box& hp = dl.cmds()[1].box;
    CHECK(hp.rect.w == 48 && hp.image->width == 24 && hp.nearest);
  }
  std::filesystem::remove_all(cache);
}

TEST(a_reported_hit_shows_the_red_marker_fading_by_the_square_of_its_age) {
  // cl_hit_markers 2 and scr_hit_marker_time 500 (the defaults): a new hit in the recording draws
  // the pack's marker at the centre in red, fading as 1 - f^2; the count going back (a seek, a new
  // recording) starts nothing.
  ge::classic::Q2FxSwitches fx;
  cl::Q2Bar bar;
  bar.fx = &fx;
  cl::BarPicture mark;
  mark.image.width = mark.image.height = 16;
  mark.image.rgba.assign(size_t{16} * 16 * 4, 255);
  bar.pictures[shooters_edge::hud::layout_picture_path("marker")] = mark;
  ge::ui::DrawList dl;
  dl.begin(640, 480);
  bar.now = 10;
  cl::draw_q2_hit_marker(dl, bar, 0, 640, 480);
  CHECK(dl.cmds().empty());  // no hit yet
  cl::draw_q2_hit_marker(dl, bar, 1, 640, 480);
  CHECK(dl.cmds().size() == 1);
  CHECK(ge::classic::q2_hit_marker_alpha(fx, 0) == 1.0f);
  CHECK(ge::classic::q2_hit_marker_alpha(fx, 0.25) == 0.75f);  // half the time: 1 - 1/4
  CHECK(ge::classic::q2_hit_marker_alpha(fx, 0.6) == 0.0f);
  bar.now = 10.6;  // past the 500 ms
  cl::draw_q2_hit_marker(dl, bar, 1, 640, 480);
  CHECK(dl.cmds().size() == 1);
  cl::draw_q2_hit_marker(dl, bar, 0, 640, 480);  // a seek back: nothing new
  CHECK(dl.cmds().size() == 1);
  fx.hit_markers = 0;  // off: no marker for a new hit
  cl::draw_q2_hit_marker(dl, bar, 1, 640, 480);
  CHECK(dl.cmds().size() == 1);
}

TEST(scr_showturtle_writes_a_frames_troubles_at_the_lower_left) {
  // One word a line in the alternate colour from eleven lines over the bottom; SUPPRESSED only
  // at 2; 0 writes none.
  cl::Q2Screen screen;
  cl::Q2Bar bar;
  bar.screen = &screen;
  const uint8_t all =
      cl::kQ2FrameBad | cl::kQ2FrameNoDelta | cl::kQ2FrameSuppressed;  // as a damaged frame
  cl::add_q2_turtle(bar, all, 320, 240);
  CHECK_MSG(placed(bar) == "'BADFRAME'@8,152 'NODELTA'@8,160 ", placed(bar));
  CHECK(bar.ops.size() == 2 && bar.ops[0].alt);
  bar.ops.clear();
  screen.turtle = 2;
  cl::add_q2_turtle(bar, cl::kQ2FrameOld | cl::kQ2FrameSuppressed, 320, 240);
  CHECK_MSG(placed(bar) == "'SUPPRESSED'@8,152 'OLDFRAME'@8,160 ", placed(bar));
  bar.ops.clear();
  screen.turtle = 0;
  cl::add_q2_turtle(bar, all, 320, 240);
  CHECK(bar.ops.empty());
}

TEST(gl_fontshadow_draws_each_letter_first_in_black_and_gl_bilerp_chars_smooths_it) {
  cl::Q2Screen screen;
  cl::Q2Bar bar;
  bar.screen = &screen;
  cl::BarPicture glyphs;
  glyphs.image.width = glyphs.image.height = 128;
  glyphs.image.rgba.assign(size_t{128} * 128 * 4, 255);
  bar.pictures["pics/conchars.pcx"] = glyphs;
  cl::add_q2_turtle(bar, cl::kQ2FrameNoDelta, 320, 240);  // one word: NODELTA, seven letters
  ge::ui::DrawList dl;
  dl.begin(320, 240);
  cl::draw_q2_bar(dl, bar, 1);
  CHECK_MSG(dl.cmds().size() == 7, std::to_string(dl.cmds().size()));
  screen.font_shadow = 1;
  screen.bilerp_chars = true;
  dl.begin(320, 240);
  cl::draw_q2_bar(dl, bar, 1);
  CHECK_MSG(dl.cmds().size() == 14, std::to_string(dl.cmds().size()));
}

TEST(scr_font_writes_the_bars_text_in_its_own_letters_and_falls_back_to_conchars) {
  cl::Q2Screen screen;
  ge::console::Console con;
  ge::console::Cvars& v = con.cvars();
  v.bind({.name = "scr_font", .fallback = "conchars", .kind = ge::console::CvarKind::text},
         &screen.font);
  cl::Q2Bar bar;
  bar.screen = &screen;
  bar.console = &con;
  cl::BarPicture glyphs, mine;
  glyphs.image.width = glyphs.image.height = 128;
  glyphs.image.rgba.assign(size_t{128} * 128 * 4, 255);
  mine = glyphs;
  mine.image.width = mine.image.height = 256;  // told apart by its size
  mine.image.rgba.assign(size_t{256} * 256 * 4, 255);
  bar.pictures["pics/conchars.pcx"] = glyphs;
  bar.pictures["pics/mine.pcx"] = mine;
  cl::add_q2_turtle(bar, cl::kQ2FrameNoDelta, 320, 240);
  ge::ui::DrawList dl;
  con.execute("scr_font mine");
  dl.begin(320, 240);
  cl::draw_q2_bar(dl, bar, 1);
  CHECK(!dl.cmds().empty() && dl.cmds()[0].box.image == &bar.pictures["pics/mine.pcx"].image);
  con.execute("scr_font gone");  // not in the packs: said, and set back
  dl.begin(320, 240);
  cl::draw_q2_bar(dl, bar, 1);
  CHECK(!dl.cmds().empty() && dl.cmds()[0].box.image == &bar.pictures["pics/conchars.pcx"].image);
  CHECK(screen.font == "conchars");
  CHECK_MSG(
      !con.output().empty() && con.output().back() == "Couldn't load pics/gone.pcx: not found",
      con.output().empty() ? "" : con.output().back());
}

TEST(viewsize_under_100_tiles_backtile_round_the_smaller_view) {
  CHECK(cl::q2_view_rect(320, 240, 100).w == 320);
  const cl::Q2ViewRect r = cl::q2_view_rect(320, 240, 50);
  CHECK(r.x == 80 && r.y == 60 && r.w == 160 && r.h == 120);
  cl::Q2Screen screen;
  cl::Q2Bar bar;
  bar.screen = &screen;
  bar.width = 320;
  bar.height = 240;
  cl::BarPicture tile;
  tile.image.width = tile.image.height = 64;
  tile.image.rgba.assign(size_t{64} * 64 * 4, 255);
  bar.pictures["pics/backtile.pcx"] = tile;
  ge::ui::DrawList dl;
  dl.begin(320, 240);
  cl::draw_q2_bar(dl, bar, 1);
  CHECK(dl.cmds().empty());  // 100: no border
  screen.viewsize = 50;
  dl.begin(320, 240);
  cl::draw_q2_bar(dl, bar, 1);
  float area = 0;
  for (const auto& c : dl.cmds()) {
    const ge::ui::Rect b = c.box.rect;
    area += b.w * b.h;
    const auto x0 = static_cast<float>(r.x), y0 = static_cast<float>(r.y);
    const auto x1 = static_cast<float>(r.x + r.w), y1 = static_cast<float>(r.y + r.h);
    const bool inside = b.x < x1 && b.x + b.w > x0 && b.y < y1 && b.y + b.h > y0;
    CHECK(!inside);
    CHECK(c.box.uv.x >= 0 && c.box.uv.x + c.box.uv.w <= 64.01f);  // within one tile
  }
  CHECK_MSG(area == 320.0f * 240 - 160.0f * 120, std::to_string(area));  // the border, once
}

TEST(timegraph_draws_each_frames_time_along_the_bottom_held_to_graphheight) {
  // A column a frame, a unit tall a millisecond (graphscale 1, graphshift 0), held to graphheight;
  // the newest at the bottom right of the bar's screen. debuggraph draws it without feeding it.
  cl::Q2Screen screen;
  cl::Q2Bar bar;
  bar.screen = &screen;
  bar.width = 640;
  bar.height = 480;
  ge::ui::DrawList dl;
  screen.time_graph = true;
  for (const double now : {1.0, 1.010, 1.050}) {  // frames of 10 and 40 ms
    bar.now = now;
    dl.begin(640, 480);
    cl::draw_q2_bar(dl, bar, 2);
  }
  CHECK_MSG(dl.cmds().size() == 2, std::to_string(dl.cmds().size()));
  const ge::ui::Rect newest = dl.cmds()[0].box.rect, older = dl.cmds()[1].box.rect;
  CHECK(newest.x == 638 && newest.w == 2 && newest.h == 64 && newest.y == 416);  // 40 ms: 32 high
  CHECK_MSG(older.x == 636 && std::abs(older.h - 20) < 0.01f, std::to_string(older.h));
  screen.time_graph = false;
  screen.debug_graph = true;
  bar.now = 2;
  dl.begin(640, 480);
  cl::draw_q2_bar(dl, bar, 2);
  CHECK(dl.cmds().size() == 2);  // the same two, nothing fed
  screen.debug_graph = false;
  dl.begin(640, 480);
  cl::draw_q2_bar(dl, bar, 2);
  CHECK(dl.cmds().empty());
}

TEST(gl_bilerp_pics_smooths_the_large_pictures_and_gl_gamma_scale_pics_brightens_them) {
  // The pack's health icon (24 x 24) and help screen (160 x 128), a colour of 90 each: 1 (its
  // default) smooths the help screen alone, 0 neither, 2 both; under gl_gamma_scale_pics they are
  // read again at the brightness `gamma` gives the world, and back at 1 without it.
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
  CHECK(ge::vfs::write_pak({{"pics/i_health.pcx", pcx(24, 24)}, {"pics/help.pcx", pcx(160, 128)}},
                           &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  cl::Q2Screen screen;
  screen.bilerp_pics = 1;
  double gamma = 0.5;
  cl::Q2Bar bar;
  bar.files = &files;
  bar.screen = &screen;
  bar.gamma = &gamma;
  for (const char* name : {"i_health", "help"}) {
    shooters_edge::hud::LayoutOp op;
    op.name = name;
    bar.ops.push_back(op);
  }
  const auto smooth = [&](size_t i) {
    ge::ui::DrawList dl;
    dl.begin(640, 480);
    cl::draw_q2_bar(dl, bar, 1);
    CHECK(dl.cmds().size() == 2);
    return dl.cmds().size() == 2 && !dl.cmds()[i].box.nearest;
  };
  CHECK(!smooth(0) && smooth(1));
  screen.bilerp_pics = 0;
  CHECK(!smooth(0) && !smooth(1));
  screen.bilerp_pics = 2;
  CHECK(smooth(0) && smooth(1));
  const auto red = [&] { return bar.pictures["pics/help.pcx"].image.rgba[0]; };
  CHECK(red() == 90);
  screen.gamma_pics = true;  // 255 x (90 / 255)^0.5
  smooth(0);
  CHECK_MSG(red() == 151, std::to_string(red()));
  screen.gamma_pics = false;
  smooth(0);
  CHECK(red() == 90);
}
