// LAB `se-pictures`: A GAME'S PICTURE STANDING IN FOR A MENU OR HUD PICTURE, alpha and all. The
// classic main menu (menu/game_menu.hpp) drawn on the CPU's 2D layer from the menu pictures the
// first game's adapter reads (client/game_adapter.hpp's `pictures`), twice over a painted wall:
// once from the fixture pack's own pictures, once with three picture files beside them in a second
// pack: the plaque down the left at its own size (bands of colour with see-through gaps the wall
// shows through), the main menu's strip at its own size, and the glyph sheet at twice its size, so
// the whole set is drawn from twice the pixels.
//
// The adapter's answer (the set's density, which pictures changed) is the digest, and the
// pictures are deterministic (the CPU raster).
//
// ge:summary Lab: the classic menu with PNG or TGA pictures standing in for the pack's, alpha kept
// ge:keywords lab se-pictures replacement-picture pictures menu hud plaque glyph-sheet alpha tga
// ge:keywords density game-adapter
// ge:skill ui labs
#include <string>
#include <utility>
#include <vector>

#include "assets/tga.hpp"
#include "console/console.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "ui/raster.hpp"
#include "vfs/pak.hpp"

namespace {

namespace ui = ge::ui;
namespace sm = shooters_edge::menu;
namespace cl = shooters_edge::client;

// An RGBA picture `w` x `h` painted by `paint(x, y, px)`.
template <typename Paint>
ge::assets::Image painted(int w, int h, Paint paint) {
  ge::assets::Image img;
  img.width = w;
  img.height = h;
  img.rgba.resize(size_t(w) * size_t(h) * 4);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) paint(x, y, &img.rgba[(size_t(y) * size_t(w) + size_t(x)) * 4]);
  return img;
}

// A wall of warm stone, so what is see-through shows.
ui::Image wall(int w, int h) {
  ui::Image img = ui::make_image(w, h, ui::hex(0x000000));
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      uint8_t* p = &img.rgba[(size_t(y) * size_t(w) + size_t(x)) * 4];
      const bool joint = (y % 24) < 2 || ((x + (y / 24 % 2) * 24) % 48) < 2;
      const uint8_t v = uint8_t(joint ? 40 : 110 + (x * 7 + y * 13) % 23);
      p[0] = v, p[1] = uint8_t(v * 0.8), p[2] = uint8_t(v * 0.6), p[3] = 255;
    }
  return img;
}

ge::lab::Picture picture_of(ui::Image img) {
  ge::lab::Picture p;
  p.width = img.width, p.height = img.height, p.rgba = std::move(img.rgba);
  return p;
}

void run(ge::lab::Lab& lab) {
  constexpr int kW = 640, kH = 400;
  std::string error;
  const auto fixture = sm::fixture_files();
  std::vector<std::pair<std::string, std::vector<uint8_t>>> listed(fixture.begin(), fixture.end());
  std::vector<uint8_t> pak;
  cl::SearchPath files;
  if (!ge::vfs::write_pak(listed, &pak, &error) ||
      !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
    lab.fail(error, "The menu's fixture pictures should pack. Skill: ui.");
    return;
  }
  const cl::GameAdapter& game = cl::classic_adapter();
  const cl::ClassicPictures own = game.pictures(game, files, false, {});
  const ui::Image* plaque = own.menu.find("qplaque");
  const ui::Image* strip = own.menu.find("mainmenu");
  if (!own.menu.loaded() || plaque == nullptr || strip == nullptr) {
    lab.fail("the fixture pack's menu pictures did not load",
             "Run tests/shooters_edge_menu_test; the fixture should carry every picture. "
             "Skill: ui.");
    return;
  }
  // The stand-ins: the plaque in bands of colour with see-through gaps, the strip's rows on a
  // dark field, the glyph sheet at twice its size (its own glyphs, each texel two pixels, tinted).
  const ge::assets::Image new_plaque =
      painted(plaque->width, plaque->height, [](int x, int y, uint8_t* p) {
        const bool gap = (y % 12) >= 8 || x < 2 || x >= 30;
        p[0] = uint8_t(40 + (y * 3) % 200), p[1] = 170, p[2] = uint8_t(230 - (y * 2) % 150);
        p[3] = gap ? 0 : 255;
      });
  const ge::assets::Image new_strip =
      painted(strip->width, strip->height, [strip](int x, int y, uint8_t* p) {
        const uint8_t* o = &strip->rgba[(size_t(y) * size_t(strip->width) + size_t(x)) * 4];
        const bool lit = o[3] > 0 && (o[0] + o[1] + o[2]) > 150;
        p[0] = lit ? 255 : 30, p[1] = lit ? 220 : 40, p[2] = lit ? 90 : 70, p[3] = o[3];
      });
  const ui::Image& sheet = own.menu.glyphs;
  const ge::assets::Image new_glyphs =
      painted(sheet.width * 2, sheet.height * 2, [&sheet](int x, int y, uint8_t* p) {
        const uint8_t* o = &sheet.rgba[(size_t(y / 2) * size_t(sheet.width) + size_t(x / 2)) * 4];
        p[0] = uint8_t(o[0] / 2 + 60), p[1] = uint8_t(o[1] / 2 + 120), p[2] = o[2];
        p[3] = o[3];
      });
  std::vector<uint8_t> extra;
  if (!ge::vfs::write_pak({{"gfx/qplaque.tga", ge::assets::encode_tga(new_plaque, true)},
                           {"gfx/mainmenu.tga", ge::assets::encode_tga(new_strip)},
                           {"gfx/conchars.tga", ge::assets::encode_tga(new_glyphs, true)}},
                          &extra, &error) ||
      !cl::add_archive(files, "pak1.pak", std::move(extra), error)) {
    lab.fail(error, "The stand-in pictures should pack. Skill: ui.");
    return;
  }
  const cl::ClassicPictures stood = game.pictures(game, files, false, {});
  lab.metric("density.own", own.menu.density, "x");
  lab.metric("density.pictures", stood.menu.density, "x");
  double changed = 0;
  for (const auto& [name, img] : stood.menu.pictures) {
    const ui::Image* was = own.menu.find(name);
    if (was == nullptr) continue;
    if (ui::scaled_nearest(*was, int(stood.menu.density)).rgba != img.rgba) changed++;
  }
  lab.metric("pictures.changed", changed, "count");
  int shot = 0;
  for (const cl::ClassicPictures* pics : {&own, &stood}) {
    ge::console::Console con;
    sm::GameMenu menu;
    menu.add_cvars(con);
    sm::Host host{.console = &con};
    menu.set_art(pics->menu);
    menu.show(host, "main");
    menu.settle();
    ui::DrawList dl;
    dl.begin(kW, kH);
    menu.draw(dl, kW, kH, 0.25);
    ui::Image img = wall(kW, kH);
    ui::render(dl, img);
    const bool first = shot++ == 0;
    lab.shot({first ? "menu-own" : "menu-pictures",
              first ? "THE CLASSIC MENU, THE PACK'S PICTURES"
                    : "THE CLASSIC MENU, PICTURE FILES STANDING IN",
              first ? "The main menu drawn from the fixture pack's own plaque, strip and glyphs."
                    : "The same menu with gfx/qplaque.tga, gfx/mainmenu.tga and gfx/conchars.tga "
                      "beside the pack: the plaque in coloured bands with see-through gaps, the "
                      "strip recoloured, the glyph sheet at twice its size.",
              first ? "the plaque down the left, the strip of rows, the cursor"
                    : "the wall showing through the plaque's gaps; the rows in yellow; the "
                      "menu the same size as the pack's",
              {{"pictures", first ? "pack" : "files"}}},
             picture_of(std::move(img)));
  }
}

const ge::lab::LabSpec kSpec{
    "se-pictures",
    ge::lab::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client/game_adapter (pictures: a PNG or TGA for a menu or HUD picture)",
    "ui",
    "Does a PNG or TGA of a menu picture's name stand in for the pack's, alpha kept, at the set's "
    "density?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
