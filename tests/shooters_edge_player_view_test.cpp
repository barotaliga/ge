// The player page's figure (games/shooters_edge/menu/player_view.hpp): the model read off the
// game's files, its stand frames, the click that fires it, the drag that turns it, the skins listed
// for the SKIN row, and the page's rows and colour picker in the menu.
//
// ge:summary Tests the player page's figure: frames, firing, dragging, skins, rows, swatches
// ge:keywords player-page figure turntable drag-to-turn swatches colour-picker skin-row
// ge:skill ui shooter
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "assets/pcx.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "morph/write.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/menu/player_view.hpp"
#include "ui/draw.hpp"
#include "vfs/pak.hpp"
#include "vfs/search.hpp"

namespace sm = shooters_edge::menu;

namespace {

// A figure with Quake's frame names: two stand frames, six shotgun attack frames, an axe's.
ge::morph::Model figure() {
  ge::morph::Model m = ge::morph::sample_model();
  const ge::morph::Frame base = m.frames.front();
  m.frames.clear();
  const auto add = [&](const std::string& name, float lift) {
    ge::morph::Frame f = base;
    f.name = name;
    f.group = static_cast<int>(m.frames.size());
    f.interval = 0;
    for (size_t i = 2; i < f.positions.size(); i += 3) f.positions[i] += lift;
    m.frames.push_back(std::move(f));
  };
  add("stand1", 0);
  add("stand2", 1);
  for (int i = 1; i <= 6; ++i) add("shotatt" + std::to_string(i), 2);
  add("axstnd1", 0);
  add("axatt1", 3);
  m.skins.resize(1);
  m.skins[0].group = 0;
  m.skins[0].interval = 0;
  return m;
}

std::vector<uint8_t> skin_pcx(uint8_t index) {
  ge::assets::IndexedImage img;
  img.width = 16;
  img.height = 8;
  img.indices.assign(size_t{16} * 8, index);
  return ge::assets::encode_pcx(img);
}

struct Rig {
  ge::vfs::SearchPath sp;
  std::vector<uint8_t> palette = std::vector<uint8_t>(768, 0);
  Rig() {
    std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {
        {"progs/player.mdl", ge::morph::write_model(figure())},
        {"skins/base.pcx", skin_pcx(16)},
        {"skins/red.pcx", skin_pcx(96)},
        {"skins/blue.pcx", skin_pcx(96)}};
    std::vector<uint8_t> pak;
    std::string error;
    CHECK_MSG(ge::vfs::write_pak(files, &pak, &error), error);
    CHECK_MSG(ge::vfs::add_archive(&sp, "pak0.pak", pak, "id1", &error), error);
    for (size_t i = 0; i < 256; ++i) palette[i * 3] = palette[i * 3 + 1] = static_cast<uint8_t>(i);
  }
};

}  // namespace

TEST(the_figure_stands_fires_once_and_returns_to_standing) {
  Rig r;
  sm::PlayerView v;
  v.set_files(&r.sp, r.palette);
  v.set_look({.top = 4, .bottom = 12, .weapon = 1});
  CHECK(v.ready());
  v.hold(0, 0);
  const ge::ui::Image* pic = v.picture(64, 96);
  CHECK(pic != nullptr && pic->width == 64 && pic->height == 96);
  CHECK(v.frame() == 0);  // stand1
  int opaque = 0;
  for (size_t i = 3; pic != nullptr && i < pic->rgba.size(); i += 4) opaque += pic->rgba[i] > 0;
  CHECK(opaque > 100);
  v.fire();
  CHECK(v.firing());
  v.update({}, {}, 0.25);
  v.picture(64, 96);
  CHECK(v.frame() >= 2 && v.frame() <= 7);  // a shotgun attack frame
  v.update({}, {}, 0.5);
  CHECK(!v.firing());         // six frames at ten a second
  v.set_look({.weapon = 0});  // the axe: its own stand and attack
  CHECK(v.ready());
  v.picture(64, 96);
  CHECK(v.frame() == 8);
  v.fire();
  v.update({}, {}, 0.01);
  v.picture(64, 96);
  CHECK(v.frame() == 9);
}

TEST(a_drag_turns_the_figure_and_a_click_fires_it) {
  Rig r;
  sm::PlayerView v;
  v.set_files(&r.sp, r.palette);
  v.set_look({.weapon = 1});
  CHECK(v.ready());
  v.hold(0, 0);
  const ge::ui::Rect area{100, 100, 50, 80};
  v.update({.x = 110, .y = 120, .held = true}, area, 0.016);  // pressed on the figure
  CHECK(v.dragging());
  v.update({.x = 150, .y = 120, .held = true}, area, 0.016);  // dragged 40 px right
  CHECK(std::fabs(v.yaw() - 20) < 1e-9);
  v.update({.x = 150, .y = 120}, area, 0.016);  // let go far from the press: no shot
  CHECK(!v.dragging() && !v.firing());
  v.update({.x = 120, .y = 130, .held = true}, area, 0.016);
  v.update({.x = 121, .y = 130}, area, 0.016);  // a press let go where it was: a click
  CHECK(v.firing());
  v.update({.x = 10, .y = 10, .held = true}, area, 0.016);  // pressed off the figure
  CHECK(!v.dragging());
}

TEST(the_skin_row_lists_the_skins_with_base_first) {
  Rig r;
  CHECK(sm::quake_skins(r.sp) == std::vector<std::string>({"base", "blue", "red"}));
  CHECK(sm::figure_weapons(false).size() == 8 && sm::figure_weapons(true).size() == 11);
  CHECK(sm::figure_weapons(false)[0] == "AXE");
}

TEST(setup_page_draws_the_live_figure_and_its_swatches_set_the_colours) {
  Rig r;
  const auto files = sm::fixture_files();
  sm::ClassicArt art;
  std::string error;
  CHECK_MSG(sm::load_classic_art(
                [&files](std::string_view path, std::vector<uint8_t>&) -> std::span<const uint8_t> {
                  const auto it = files.find(path);
                  if (it == files.end()) return {};
                  return it->second;
                },
                &art, &error),
            error);
  ge::console::Console con;
  con.cvars().add({.name = "topcolor", .fallback = "0"});
  con.cvars().add({.name = "bottomcolor", .fallback = "0"});
  con.cvars().add({.name = "skin", .fallback = "", .kind = ge::console::CvarKind::text});
  con.cvars().add({.name = "name", .fallback = "player", .kind = ge::console::CvarKind::text});
  sm::GameMenu g;
  g.add_cvars(con);
  sm::Host host{.console = &con, .files = &r.sp};
  g.set_art(art);
  g.show(host, "setup");
  const ge::ui::Page* page = g.main().page("setup");
  CHECK(page != nullptr && page->find("qwskin") != nullptr && page->find("figweapon") != nullptr);
  if (page == nullptr) return;
  CHECK(page->find("qwskin")->choices == std::vector<std::string>({"base", "blue", "red"}));
  CHECK(page->find("figweapon")->choices.size() == 8);
  g.update({}, false, host, 0.016);
  ge::ui::DrawList dl;
  dl.begin(640, 400);
  g.draw(dl, 640, 400, 0.25);
  CHECK(g.problems().empty());
  CHECK(g.figure_box().w > 0 && g.figure_box().h > 0);
  CHECK(g.swatches().w > 0);
  // A click on the pants row's sixth swatch: bottomcolor 5.
  const ge::ui::Rect s = sm::swatch_rect(g.swatches(), 1, 5);
  ge::ui::MenuInput in;
  in.click = true;
  in.pointer_x = s.x + s.w / 2;
  in.pointer_y = s.y + s.h / 2;
  g.update(in, false, host, 0.016);
  const ge::console::Cvar* bottom = con.cvars().find("bottomcolor");
  CHECK(bottom != nullptr && bottom->value == "5");
  CHECK(g.main().page("setup")->find("bottomcolor")->choice == 5);
}
