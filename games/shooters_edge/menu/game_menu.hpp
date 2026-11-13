// THE MENU OVER THE GAME: what the window holds to open the main menu with ESCAPE, drive it with
// the player's presses, and draw it over the world each frame, in one of two looks:
//
// - **classic**: the pack's own pictures (classic_view.hpp), when the packs on the search path
//   carry the classic menu's art (classic_art.hpp) and `menu_look` is `auto` or `classic`;
// - **modern**: the engine's menu view (ui/menu_view.hpp) over the darkened game, the game's name
//   at the top of the column, the dimmed rows' reason in the footer.
//
// OPTIONS ▸ CLASSIC ENHANCED holds the game the player loads and its enhancing:
//
// - **LOAD GAME** asks the window for the desktop's folder chooser (`take_load`); the window
//   checks the folder, opens the install (client/game_folder.hpp), says which one with `set_game`,
//   and starts its enhancing with `enhance_game` when `ce_auto` is on (the default).
// - **ENHANCE** shows the enhancing, starting a run of the loaded install when none is running
//   (each stage skipped when it is current): classic-enhance's own page (plugins/classicenhance/
//   menu.hpp) in the main menu's place, its progress panel beside CANCEL and PLAY (which closes
//   the menu: the window draws the world again from the cache when a run ends, `take_ended`). BACK
//   returns to CLASSIC ENHANCED. It keeps the modern look in both looks, since its progress panel
//   is drawn for that view. The run goes on in the background with the menu closed.
//
// OPTIONS ▸ SCREENSHOTS shows the screenshots F12 took (gallery_page.hpp over the engine's
// gallery, capture/gallery.hpp): the window names their folder with `set_screenshots`; the arrows
// and the pointer move through them, ENTER views one, DELETE (or BACKSPACE, a Mac's delete) asks
// to delete it, ESCAPE goes back to OPTIONS. It keeps the modern look in both looks.
//
// MULTIPLAYER ▸ JOIN A GAME shows the server browser (servers_page.hpp over the engine's server
// list, net/server_list.hpp): the window hands it the game's protocol with `servers().set_query`
// and the loaded install's masters with `servers().set_masters`; ENTER on a server runs
// `connect <address>` and closes the menu, R or F5 refreshes, ESCAPE goes back to MULTIPLAYER. It
// keeps the modern look in both looks.
//
// DEMOS (on the root) shows the demos page (demos_page.hpp over the engine's folder browser,
// vfs/browse.hpp): the window hands it the game's recording extensions, the game's folder and its
// check of a picked file (`demos()`); a demo that passes the check is handed back by `take_demo`
// and the menu closes, the window playing it as `playdemo` does. It keeps the modern look in both
// looks.
//
// THE SOUNDS: each frame's presses leave one cue (ui/menu.hpp's `MenuCue`) the window plays from
// the game's own recordings (client/menu_sound.hpp): the menu opened (Escape, `togglemenu`) or a
// page or item entered is `enter`; the focus moved, or a page of the screenshots, servers, demos
// or help flipped, is `move`; a value changed or a key bound is `change`; back out of a page and
// the menu closed is `back`.
//
// The commands it registers: `togglemenu` (what ESCAPE does), `menu_main`, `menu_options`,
// `menu_quit`, `menu_screenshots`, `menu_servers`, `menu_demos` (the menu opened on that page);
// the cvars
// `menu_look` (auto, classic, modern) and `ce_auto` (1: a game loaded is enhanced in the background
// at once). The game keeps running under the menu (the listen server's frames go on); the player's
// keys and mouse go to the menu while it is open.
//
// ge:summary The main menu over the game: ESCAPE, the presses, the classic or modern look, enhance
// ge:keywords menu main-menu escape togglemenu menu_look classic modern look overlay pointer
// ge:keywords classic-enhance enhance job switches help-pages window shooters-edge press_key
// ge:keywords waiting-key rebind customize-controls load-game take_load enhance_game ce_auto
// ge:keywords background-enhance take_ended screenshots gallery menu_screenshots set_screenshots
// ge:keywords server-browser join-a-game menu_servers servers demos menu_demos take_demo
// ge:keywords menu-sound menu-cue take_cue click
// ge:skill ui console classicenhance
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "capture/gallery.hpp"
#include "classicenhance/menu.hpp"
#include "classicenhance/progress.hpp"
#include "classicenhance/switches.hpp"
#include "console/console.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/menu/classic_view.hpp"
#include "shooters_edge/menu/demos_page.hpp"
#include "shooters_edge/menu/gallery_page.hpp"
#include "shooters_edge/menu/main_menu.hpp"
#include "shooters_edge/menu/servers_page.hpp"
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
  // LOAD GAME was pressed since the last call: the window shows the desktop's folder chooser.
  bool take_load() { return std::exchange(load_asked_, false); }
  // The folder picked was not loaded: `why` shown as LOAD GAME's hint until a game loads.
  void load_refused(std::string why);
  // The install the window has open (its folder; "" none): what ENHANCE enhances.
  void set_game(std::string basedir);
  const std::string& game() const { return game_; }
  // `ce_auto`: a game loaded is enhanced at once.
  bool auto_enhance() const { return auto_; }
  // A run of the loaded install started (one of another install stopped first; one of this
  // install running is kept); with `watch` the menu, when open, shows it. False with the reason,
  // which the enhancing page shows too.
  bool enhance_game(bool watch, std::string* error);
  // The run, null before the first.
  const ge::classicenhance::EnhanceJob* job() const { return job_.get(); }
  // True once, after a run ended (finished, failed or cancelled).
  bool take_ended();

  bool open() const { return open_; }
  // Opens on `page` (refreshed from `host`), or closes.
  void show(Host& host, std::string_view page = "main");
  void hide();

  // One frame: `escape` pressed while the menu is closed opens it (so does `togglemenu`); while it
  // is open the presses in `in` (Escape is its `back`) drive it. `dt` steps the animations.
  void update(const ge::ui::MenuInput& in, bool escape, Host& host, double dt);
  // The menu over a `width` x `height` frame; `t` turns the classic cursor.
  void draw(ge::ui::DrawList& list, int width, int height, double t);

  // CUSTOMIZE CONTROLS waits for a key (a row was pressed): the window hands it every key, mouse
  // button and wheel turn that goes down, by its bind name, to `press_key`, and none of its
  // presses to `update`. `escape` stops waiting, `backspace` and `del` clear the move's keys, any
  // other binds it (the frame the row was pressed on is skipped, so its own ENTER binds nothing).
  bool waiting_key() const { return !waiting_.empty(); }
  void press_key(std::string_view key, Host& host);

  // The folder the game's screenshots are written to: what OPTIONS ▸ SCREENSHOTS shows.
  void set_screenshots(std::string dir) { gallery_.set_dir(std::move(dir)); }
  ge::capture::Gallery& gallery() { return gallery_; }
  // Whether the screenshots page is the one up.
  bool in_gallery() const { return in_gallery_; }
  // The server browser (JOIN A GAME), and whether it is the page up.
  ServersPage& servers() { return servers_; }
  bool in_servers() const { return in_servers_; }
  // The demos page (DEMOS), whether it is the page up, and the demo it asked to play since the
  // last call ("" none): the window plays it.
  DemosPage& demos() { return demos_; }
  bool in_demos() const { return in_demos_; }
  std::string take_demo() { return std::exchange(demo_ask_, std::string()); }

  // The sound this frame's presses (and opening or closing) made since the last call; none after.
  ge::ui::MenuCue take_cue() { return std::exchange(cue_, ge::ui::MenuCue::none); }

  // The next draw settles every animation where it is going (a lab's still).
  void settle() { settle_ = true; }
  // The look the menu draws in now.
  bool classic() const;
  // Whether classic-enhance's menu is the one up.
  bool in_enhance() const { return in_enhance_; }
  ge::ui::Menu& main() { return main_; }
  // The game's own menu (its adapter's `menu`) in place of the first game's: the pages rebuilt.
  // The host's `table` must name the same table.
  void set_table(const MenuTable& table) { main_ = build_menu(table); }
  ge::ui::Menu& enhance() { return enhance_; }
  int help_page() const { return help_page_; }
  // What the layout drawn last found wrong (layout_problems or classic_problems).
  const std::vector<std::string>& problems() const { return problems_; }

 private:
  // The run's board, or a failed one saying why the last run did not start.
  ge::classicenhance::BoardView board() const;
  void watch_enhancing();
  void refresh_main(const Host& host);
  void apply_enhance(const ge::ui::MenuEvent& e, Host& host);
  void show_gallery();
  void update_gallery(const ge::ui::MenuInput& in, Host& host);
  void show_servers();
  void update_servers(const ge::ui::MenuInput& in, Host& host);
  void show_demos();
  void update_demos(const ge::ui::MenuInput& in, Host& host);

  ge::console::Console* con_ = nullptr;
  std::string look_ = "auto";
  ClassicArt art_;
  ge::ui::Menu main_;
  ge::ui::MenuView view_;
  ge::ui::MenuStyle style_;
  std::vector<ge::ui::Rect> hits_;  // the last layout's rows, for the pointer
  std::vector<std::string> problems_;
  bool open_ = false;
  ge::ui::MenuCue cue_ = ge::ui::MenuCue::none;  // the sound not yet taken
  void cue(ge::ui::MenuCue c) { cue_ = ge::ui::louder(cue_, c); }
  bool toggle_ = false;
  bool settle_ = false;
  std::string_view toggle_page_ = "main";  // a literal: the commands' pages
  int help_page_ = 0;
  double dt_ = 1.0 / 60;  // the last frame's, s

  std::string waiting_;         // the CUSTOMIZE CONTROLS row waiting for a key, or ""
  bool waiting_fresh_ = false;  // it started this frame

  bool in_enhance_ = false;
  ge::classicenhance::Switches switches_;
  bool auto_ = true;         // ce_auto
  bool load_asked_ = false;  // LOAD GAME pressed, the window not told yet
  std::string game_;         // the install loaded
  std::string load_note_;    // why the last folder picked was not loaded
  std::string job_game_;     // the install the run is of
  std::string start_error_;  // why the last run did not start
  bool job_live_ = false;    // a run started and its end not taken
  int pack_ = 0;             // the classic-enhance menu's pack choice (one: the game)
  ge::ui::Menu enhance_;
  std::unique_ptr<ge::classicenhance::EnhanceJob> job_;
  std::vector<ge::ui::Image> thumbs_;

  ge::capture::Gallery gallery_;
  GalleryPage gallery_page_;
  ServersPage servers_;
  DemosPage demos_;
  std::string demo_ask_;  // the demo picked, the window not told yet
  bool in_gallery_ = false;
  bool in_servers_ = false;
  bool in_demos_ = false;
};

}  // namespace shooters_edge::menu
