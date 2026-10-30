// THE MENU OVER THE GAME: what the window holds to open the main menu with ESCAPE, drive it with
// the player's presses, and draw it over the world each frame, in one of two looks:
//
// - **classic**: the pack's own pictures (classic_view.hpp), when the packs on the search path
//   carry the classic menu's art (classic_art.hpp) and `menu_look` is `auto` or `classic`;
// - **modern**: the engine's menu view (ui/menu_view.hpp) over the darkened game, the game's name
//   at the top of the column, the dimmed rows' reason in the footer.
//
// OPTIONS ▸ CLASSIC ENHANCE opens classic-enhance's own menu (plugins/classicenhance/menu.hpp) in
// its place: the pack to enhance (the packs on the search path), ENHANCE run on a thread into the
// local cache and watched stage by stage, every `ce_` switch on its own page. Back at its root
// returns to OPTIONS. Its pages keep the modern look in both looks, since its progress panel is
// drawn for that view.
//
// The commands it registers: `togglemenu` (what ESCAPE does), `menu_main`, `menu_options`,
// `menu_quit` (the menu opened on that page); the cvar `menu_look` (auto, classic, modern).
// The game keeps running under the menu (the listen server's frames go on); the player's keys and
// mouse go to the menu while it is open.
//
// ge:summary The main menu over the game: ESCAPE, the presses, the classic or modern look, enhance
// ge:keywords menu main-menu escape togglemenu menu_look classic modern look overlay pointer
// ge:keywords classic-enhance enhance job switches help-pages window shooters-edge
// ge:skill ui console classicenhance
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "classicenhance/menu.hpp"
#include "classicenhance/progress.hpp"
#include "classicenhance/switches.hpp"
#include "console/console.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/classic_view.hpp"
#include "shooters_edge/menu/main_menu.hpp"
#include "ui/draw.hpp"
#include "ui/menu.hpp"
#include "ui/menu_view.hpp"

namespace shooters_edge::menu {

class GameMenu {
 public:
  GameMenu();
  GameMenu(const GameMenu&) = delete;
  GameMenu& operator=(const GameMenu&) = delete;
  ~GameMenu();

  // Registers `menu_look`, the menu's commands and the `ce_` switches on `con`, which must outlive
  // the menu.
  void add_cvars(ge::console::Console& con);
  // The classic pictures (empty: the classic look is not offered).
  void set_art(ClassicArt art);
  const ClassicArt& art() const { return art_; }
  // The classic packs classic-enhance offers to enhance (paths on disk).
  void set_packs(std::vector<std::string> paths);

  bool open() const { return open_; }
  // Opens on `page` (refreshed from `host`), or closes.
  void show(Host& host, std::string_view page = "main");
  void hide();

  // One frame: `escape` pressed while the menu is closed opens it (so does `togglemenu`); while it
  // is open the presses in `in` (Escape is its `back`) drive it. `dt` steps the animations.
  void update(const ge::ui::MenuInput& in, bool escape, Host& host, double dt);
  // The menu over a `width` x `height` frame; `t` turns the classic cursor.
  void draw(ge::ui::DrawList& list, int width, int height, double t);

  // The next draw settles every animation where it is going (a lab's still).
  void settle() { settle_ = true; }
  // The look the menu draws in now.
  bool classic() const;
  // Whether classic-enhance's menu is the one up.
  bool in_enhance() const { return in_enhance_; }
  ge::ui::Menu& main() { return main_; }
  ge::ui::Menu& enhance() { return enhance_; }
  int help_page() const { return help_page_; }
  // What the layout drawn last found wrong (layout_problems or classic_problems).
  const std::vector<std::string>& problems() const { return problems_; }

 private:
  void start_enhance();
  void apply_enhance(const ge::ui::MenuEvent& e, Host& host);

  ge::console::Console* con_ = nullptr;
  std::string look_ = "auto";
  ClassicArt art_;
  ge::ui::Menu main_;
  ge::ui::MenuView view_;
  ge::ui::MenuStyle style_;
  std::vector<ge::ui::Rect> hits_;  // the last layout's rows, for the pointer
  std::vector<std::string> problems_;
  bool open_ = false;
  bool toggle_ = false;
  bool settle_ = false;
  std::string toggle_page_ = "main";
  int help_page_ = 0;
  double dt_ = 1.0 / 60;  // the last frame's, s

  bool in_enhance_ = false;
  ge::classicenhance::Switches switches_;
  std::vector<std::string> packs_;
  int pack_ = 0;
  ge::ui::Menu enhance_;
  std::unique_ptr<ge::classicenhance::EnhanceJob> job_;
  std::vector<ge::ui::Image> thumbs_;
};

}  // namespace shooters_edge::menu
