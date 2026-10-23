// engine/ui's menus: the focus walks the stops and wraps, presses do what each item kind does,
// pages open and close as a stack, the pointer hovers and clicks, and the view lays a page out
// inside the screen at every reference viewport with nothing to flag.
//
// ge:summary Tests menus: focus, presses, adjustments, the page stack, the pointer, the layout
// ge:keywords test ui menu focus page stack toggle choice slider pointer layout problems viewport
// ge:skill ui testing
#include <string>
#include <vector>

#include "check.hpp"
#include "ui/menu_view.hpp"

using namespace ge::ui;

namespace {

Menu demo() {
  Menu m;
  Page& root = m.add_page("root");
  root.items = {{.id = "play", .text = "PLAY", .prominent = true},
                {.id = "options", .text = "OPTIONS", .opens = "options"},
                {.id = "soon", .text = "ONLINE", .disabled = true},
                {.id = "quit", .text = "QUIT"}};
  Page& opt = m.add_page("options", "OPTIONS");
  opt.items = {{.id = "head", .text = "SOUND", .kind = ItemKind::label},
               {.id = "sound", .text = "SOUND", .kind = ItemKind::toggle, .on = true},
               {.id = "volume",
                .text = "VOLUME",
                .kind = ItemKind::slider,
                .value = 0.8,
                .min = 0,
                .max = 1,
                .step = 0.1},
               {.id = "camera",
                .text = "CAMERA",
                .kind = ItemKind::choice,
                .choices = {"CHASE", "HELMET", "TIPS"}},
               {.id = "back", .text = "BACK", .kind = ItemKind::back}};
  return m;
}

MenuInput press(bool MenuInput::*k) {
  MenuInput in;
  in.*k = true;
  return in;
}

}  // namespace

TEST(focus_starts_on_the_first_stop_and_skips_disabled_items) {
  Menu m = demo();
  m.input({});
  CHECK(m.focused()->id == "play");
  m.input(press(&MenuInput::down));
  m.input(press(&MenuInput::down));
  CHECK(m.focused()->id == "quit");  // ONLINE is disabled
  m.input(press(&MenuInput::down));
  CHECK(m.focused()->id == "play");  // wraps
  m.input(press(&MenuInput::up));
  CHECK(m.focused()->id == "quit");
}

TEST(an_action_reports_itself_and_opens_its_page) {
  Menu m = demo();
  m.input({});
  auto ev = m.input(press(&MenuInput::accept));
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::activated && ev[0].item == "play");
  m.focus("options");
  ev = m.input(press(&MenuInput::accept));
  CHECK(ev.size() == 2 && ev[1].kind == EventKind::opened && ev[1].page == "options");
  CHECK(m.top().id == "options" && m.stack().size() == 2);
  CHECK(m.focused()->id == "sound");  // the label is not a stop
}

TEST(items_adjust_by_kind) {
  Menu m = demo();
  m.open("options");
  m.input({});
  auto ev = m.input(press(&MenuInput::accept));
  CHECK(!m.top().find("sound")->on && ev[0].kind == EventKind::changed);
  m.focus("volume");
  m.input(press(&MenuInput::right));
  m.input(press(&MenuInput::right));
  m.input(press(&MenuInput::right));
  CHECK(m.top().find("volume")->value == 1.0);  // clamped at max
  ev = m.input(press(&MenuInput::right));
  CHECK(ev.empty());  // no change, no event
  m.input(press(&MenuInput::left));
  CHECK(item_value(*m.top().find("volume")) == "0.9");
  m.focus("camera");
  m.input(press(&MenuInput::left));
  CHECK(item_value(*m.top().find("camera")) == "TIPS");  // wraps backwards
}

TEST(back_closes_pages_and_reports_at_the_root) {
  Menu m = demo();
  m.open("options");
  auto ev = m.input(press(&MenuInput::back));
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::closed && m.at_root());
  ev = m.input(press(&MenuInput::back));
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::back_at_root);
  m.open("options");
  m.focus("back");
  m.input(press(&MenuInput::accept));
  CHECK(m.at_root());
}

TEST(the_pointer_hovers_and_clicks_what_is_under_it) {
  Menu m = demo();
  MenuStyle st;
  const MenuLayout L = layout_menu(m, 1280, 720, st);
  const Rect q = L.items[3];
  MenuInput in;
  in.pointer_moved = true;
  in.pointer_x = q.x + 5, in.pointer_y = q.y + 5;
  m.input(in, L.items);
  CHECK(m.focused()->id == "quit");
  // Over the disabled item: the focus stays.
  in.pointer_y = L.items[2].y + 5;
  m.input(in, L.items);
  CHECK(m.focused()->id == "quit");
  in.pointer_y = q.y + 5;
  in.click = true;
  auto ev = m.input(in, L.items);
  CHECK(ev.size() == 1 && ev[0].item == "quit");
}

TEST(every_page_reads_at_every_reference_viewport) {
  const int sizes[][2] = {{1280, 720}, {390, 844}, {844, 390}, {1920, 1080}, {2732, 2048}};
  MenuStyle st;
  st.logo = "FALL LINE";
  for (const auto& wh : sizes) {
    for (const char* page : {"root", "options"}) {
      Menu m = demo();
      if (std::string(page) != "root") m.open(page);
      const MenuLayout L = layout_menu(m, wh[0], wh[1], st);
      const auto problems = layout_problems(m, L, st);
      for (const auto& p : problems)
        CHECK_MSG(false, std::to_string(wh[0]) + "x" + std::to_string(wh[1]) + ": " + p);
      CHECK(L.portrait == (wh[1] > wh[0]));
      CHECK(L.items.size() == m.top().items.size());
    }
  }
}

TEST(a_page_too_long_for_the_screen_is_scaled_to_fit) {
  Menu m;
  Page& p = m.add_page("long", "MANY");
  for (int i = 0; i < 14; i++) p.items.push_back({.id = "i" + std::to_string(i), .text = "ITEM"});
  MenuStyle st;
  const MenuLayout L = layout_menu(m, 844, 390, st);
  CHECK(L.scale < 0.72f);
  CHECK(layout_problems(m, L, st).empty());
}

TEST(layout_problems_name_text_too_wide_for_its_slab) {
  Menu m;
  m.add_page("root").items = {
      {.id = "long", .text = "A VERY LONG BUTTON LABEL THAT GOES ON AND ON"}};
  MenuStyle st;
  const auto problems = layout_problems(m, layout_menu(m, 1280, 720, st), st);
  CHECK(problems.size() == 1 && problems[0].find("'long'") != std::string::npos);
}

TEST(the_highlight_glides_to_the_focus_and_draws) {
  Menu m = demo();
  MenuStyle st;
  const MenuLayout L = layout_menu(m, 1280, 720, st);
  MenuView v;
  m.input({});
  v.settle(m, L);
  m.input(press(&MenuInput::down));
  v.frame(m, L, 1.0 / 60);
  DrawList dl;
  dl.begin(1280, 720);
  v.draw(dl, m, L, st);
  CHECK(dl.size() > 8);
  for (int i = 0; i < 60; i++) v.frame(m, L, 1.0 / 60);
  CHECK(v.page_in() == 1);
}
