// LAB `se-remix-menu`: CLASSIC REMIX'S `cr_menu` AND `cr_charset`. The played game's main menu
// and its options page drawn on the CPU's 2D layer four ways: its own; with `cr_menu` naming the
// other game (that game's pictures, the played game's letters); with `cr_charset` naming it (the
// played game's pictures, that game's letters); and with both. The pictures are read as the window
// reads them (client/remix_pictures.hpp's `remixed_pictures`) from the player's own packs.
//
// `--game=<install>/<base folder>` names the played game's folder and `--remix=<install>` the
// other full game's install (`--remix-game`, Quake II by default). Without them (the smoke run)
// Quake's fixture pictures are drawn alone, as their own menu.
//
// The digest is each variant's picture sizes, densities and whether a glyph sheet changed.
//
// ge:summary Lab: the menu drawn with another game's pictures (cr_menu) and letters (cr_charset)
// ge:keywords lab se-remix-menu remix cr_menu cr_charset menu glyph-sheet conchars pictures
// ge:skill ui labs
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/client/remix_pictures.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "ui/raster.hpp"
#include "vfs/pak.hpp"

namespace {

namespace ui = ge::ui;
namespace sm = shooters_edge::menu;
namespace cl = shooters_edge::client;

ge::lab::Picture picture_of(ui::Image img) {
  ge::lab::Picture p;
  p.width = img.width, p.height = img.height, p.rgba = std::move(img.rgba);
  return p;
}

// The menu on page `page` drawn from `art` over black, `w` x `h`.
ui::Image drawn(const sm::ClassicArt& art, std::string_view page, int w, int h) {
  ge::console::Console con;
  sm::GameMenu menu;
  menu.add_cvars(con);
  sm::Host host{.console = &con};
  menu.set_art(art);
  menu.show(host, page);
  menu.settle();
  ui::DrawList dl;
  dl.begin(w, h);
  menu.draw(dl, w, h, 0.25);
  ui::Image img = ui::make_image(w, h, ui::hex(0x101010));
  ui::render(dl, img);
  return img;
}

void run(ge::lab::Lab& lab) {
  constexpr int kW = 640, kH = 480;
  std::string error;
  cl::SearchPath files;
  cl::RemixFiles remix;
  std::string_view played = "q1";
  const std::string& folder = lab.arg("game");
  if (folder.empty()) {
    const auto fixture = sm::fixture_files();
    std::vector<std::pair<std::string, std::vector<uint8_t>>> listed(fixture.begin(),
                                                                     fixture.end());
    std::vector<uint8_t> pak;
    if (!ge::vfs::write_pak(listed, &pak, &error) ||
        !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "The menu's fixture pictures should pack. Skill: ui.");
      return;
    }
  } else {
    const size_t cut = folder.find_last_of('/');
    const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
    const std::string leaf = cut == std::string::npos ? folder : folder.substr(cut + 1);
    for (const ge::classicenhance::Profile& p : ge::classicenhance::kProfiles)
      if (p.base_dir == leaf) played = p.id;
    if (!cl::open_game_dir(base, leaf, files, error)) {
      lab.fail(error, "Read the reason above. Skill: shooter.");
      return;
    }
    if (const std::string& other = lab.arg("remix"); !other.empty()) {
      cl::RemixState state;
      state.on = true;
      state.other = lab.arg("remix-game").empty() ? "q2" : lab.arg("remix-game");
      state.other_basedir = other;
      if (!cl::open_remix(remix, state, played, base, leaf, error)) {
        lab.fail(error, "Read the reason above. Skill: shooter.");
        return;
      }
    }
  }
  const cl::GameAdapter& game = cl::adapter_of(*shooters_edge::profiles::find_game(played));
  const std::string other = remix.state.on ? remix.state.other : std::string(played);
  struct Variant {
    const char* id;
    const char* title;
    bool menu, charset;
  };
  constexpr std::array<Variant, 4> kVariants{
      {{"own", "ITS OWN MENU", false, false},
       {"menu", "CR_MENU: THE OTHER GAME'S MENU", true, false},
       {"charset", "CR_CHARSET: THE OTHER GAME'S LETTERS", false, true},
       {"both", "BOTH SWAPPED", true, true}}};
  const ui::Image* own_glyphs = nullptr;
  cl::ClassicPictures first;
  for (const Variant& v : kVariants) {
    if (!remix.state.on && v.id != std::string_view("own")) continue;
    remix.menu = v.menu ? std::string_view(other) : played;
    remix.charset = v.charset ? std::string_view(other) : played;
    cl::ClassicPictures pics = cl::remixed_pictures(files, remix, false, game);
    const std::string id = v.id;
    lab.metric(id + ".density", pics.menu.density, "x");
    lab.metric(id + ".pictures", static_cast<double>(pics.menu.pictures.size()), "count");
    lab.metric(id + ".plates", pics.menu.plates() ? 1 : 0, "bool");
    if (own_glyphs == nullptr) {
      first = pics;
      own_glyphs = &first.menu.glyphs;
    }
    lab.metric(id + ".letters_swapped", pics.menu.glyphs.rgba != own_glyphs->rgba ? 1 : 0, "bool");
    for (const std::string_view page : {"main", "options"}) {
      const std::string name = id + "-" + std::string(page);
      lab.shot({name,
                std::string(v.title) + (page == "main" ? ", MAIN" : ", OPTIONS"),
                "The played game's (" + std::string(played) + ") menu page '" + std::string(page) +
                    "' with cr_menu " + (v.menu ? other : "own") + " and cr_charset " +
                    (v.charset ? other : "own") + ".",
                "the plaque, the titles and the cursor follow cr_menu; the rows' letters on the "
                "options page follow cr_charset",
                {{"variant", id}, {"page", std::string(page)}}},
               picture_of(drawn(pics.menu, page, kW, kH)));
    }
  }
}

const ge::lab::LabSpec kSpec{
    "se-remix-menu",
    ge::lab::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client/remix_pictures (cr_menu, cr_charset)",
    "ui",
    "Does cr_menu draw the other game's menu pictures and cr_charset write in its letters, each "
    "apart from the other?",
    {{"game", "", "the played game's folder (<install>/<base folder>); empty: the fixture menu"},
     {"remix", "", "with --game: the other full game's install, classic remix on"},
     {"remix-game", "q2", "the other game's id"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
