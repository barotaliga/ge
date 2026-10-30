// The classic-enhance menu (plugins/classicenhance/menu.hpp): its pages; a toggle pressed sets its
// ce_ cvar and a group's ALL OFF, ALL ON and DEFAULTS set all of it; the console's change shows on
// the toggle; ENHANCE, CANCEL and PLAY come back as actions and PLAY is enabled only once the
// board is ready; the panel stands clear of the column and the footer and draws.
//
// ge:summary Tests the classic-enhance menu's pages, toggles through cvars, actions and the panel
// ge:keywords test classicenhance menu page toggle cvar enhance cancel play panel progress
// ge:skill classicenhance testing
#include <string>
#include <vector>

#include "check.hpp"
#include "classicenhance/menu.hpp"
#include "ui/raster.hpp"

namespace ce = ge::classicenhance;
using namespace ge::ui;

namespace {

std::vector<MenuEvent> press(Menu& m, std::string_view id) {
  CHECK_MSG(m.focus(id), std::string(id) + " is not on page " + m.top().id);
  MenuInput in;
  in.accept = true;
  return m.input(in);
}

}  // namespace

TEST(menu_has_a_page_for_every_group_and_a_toggle_for_every_switch) {
  const std::vector<std::string> packs = {"installed game"};
  const ce::Switches values;
  Menu m = ce::classic_menu(packs, values);
  for (const char* id : {"classic", "enhancing", "switches", "switches.effects",
                         "switches.liquids-and-sky", "switches.textures"}) {
    CHECK_MSG(m.page(id) != nullptr, std::string("no page ") + id);
  }
  size_t toggles = 0;
  for (size_t g = 0; g < ce::kSwitchGroupCount; g++) {
    const Page* p = m.page(m.page("switches")->items[g].opens);
    CHECK(p != nullptr);
    if (p == nullptr) continue;
    for (const Item& it : p->items) toggles += it.kind == ItemKind::toggle;
  }
  CHECK(toggles == ce::kSwitchCount);
  CHECK(!m.page("classic")->find("enhance")->disabled);
  // No pack found: nothing to enhance.
  Menu none = ce::classic_menu({}, values);
  CHECK(none.page("classic")->find("enhance")->disabled);
}

TEST(toggles_set_their_cvars_and_follow_the_console) {
  ge::console::Cvars cvars;
  ce::Switches values;
  ce::bind_switches(cvars, &values);
  const std::vector<std::string> packs = {"a", "b"};
  Menu m = ce::classic_menu(packs, values);
  m.open("switches.effects");
  int pack = 0;
  for (const MenuEvent& e : press(m, "muzzles")) {
    CHECK(ce::apply_menu_event(e, m, cvars, &values, &pack) == ce::MenuAction::none);
  }
  CHECK(!values.muzzles && cvars.find("ce_muzzles")->value == "0");
  for (const MenuEvent& e : press(m, "all-off")) ce::apply_menu_event(e, m, cvars, &values, &pack);
  CHECK(!values.flames && !values.beams && values.decals);
  CHECK(!m.top().find("flames")->on);
  for (const MenuEvent& e : press(m, "defaults")) ce::apply_menu_event(e, m, cvars, &values, &pack);
  CHECK(values.flames && values.muzzles);
  // The console turns one off; the page follows.
  cvars.set("ce_bubbles", "0", ge::console::Source::local);
  ce::sync_switches(m, values);
  CHECK(!m.top().find("bubbles")->on);
}

TEST(enhance_cancel_and_play_are_actions_play_waits_for_ready) {
  ge::console::Cvars cvars;
  ce::Switches values;
  ce::bind_switches(cvars, &values);
  const std::vector<std::string> packs = {"a", "b"};
  Menu m = ce::classic_menu(packs, values);
  int pack = 0;
  MenuInput right;
  right.right = true;
  m.focus("pack");
  for (const MenuEvent& e : m.input(right)) ce::apply_menu_event(e, m, cvars, &values, &pack);
  CHECK(pack == 1);
  bool enhance = false;
  for (const MenuEvent& e : press(m, "enhance"))
    enhance |= ce::apply_menu_event(e, m, cvars, &values, &pack) == ce::MenuAction::enhance;
  CHECK(enhance && m.top().id == "enhancing");

  ce::BoardView board;
  board.started = true;
  ce::sync_enhancing(m, board);
  CHECK(m.top().find("play")->disabled && !m.top().find("cancel")->disabled);
  bool cancel = false;
  for (const MenuEvent& e : press(m, "cancel"))
    cancel |= ce::apply_menu_event(e, m, cvars, &values, &pack) == ce::MenuAction::cancel;
  CHECK(cancel);
  board.finished = true;
  ce::sync_enhancing(m, board);
  CHECK(!m.top().find("play")->disabled && m.top().find("cancel")->disabled);
  bool play = false;
  for (const MenuEvent& e : press(m, "play"))
    play |= ce::apply_menu_event(e, m, cvars, &values, &pack) == ce::MenuAction::play;
  CHECK(play);
}

TEST(panel_stands_clear_of_the_column_and_draws) {
  const std::vector<std::string> packs = {"a"};
  Menu m = ce::classic_menu(packs, ce::Switches{});
  m.open("enhancing");
  MenuStyle st;
  st.footer = "STAMP";
  for (const auto& [w, h] : {std::pair{1280, 720}, std::pair{390, 844}}) {
    const MenuLayout L = layout_menu(m, w, h, st);
    const Rect a = ce::panel_area(L);
    CHECK(a.w > 200 && a.h > 200);
    CHECK(a.right() <= L.column.x || a.x >= L.column.right() || a.bottom() <= L.column.y);
    CHECK(a.bottom() <= L.footer.y || a.right() <= L.footer.x);
    ce::BoardView board;
    board.started = true;
    board.stages.push_back({ce::Stage::pack, ce::StageState::done, 1, 1, "verified", 1});
    board.stages.push_back({ce::Stage::textures, ce::StageState::running, 3, 8, "wall", 2});
    DrawList dl;
    dl.begin(w, h);
    std::vector<Image> store;
    ce::draw_progress(dl, board, a, {}, store, "a");
    Image img = make_image(w, h, hex(0x000000));
    render(dl, img);
    const size_t at = (size_t(a.y + a.h / 2) * size_t(w) + size_t(a.x + 4)) * 4;
    CHECK(img.rgba[at + 2] > 0);  // the panel's blue-black, not the black under it
  }
  CHECK(ce::clock_text(5) == "5 S" && ce::clock_text(65) == "1:05" &&
        ce::clock_text(3725) == "1:02:05");
}
