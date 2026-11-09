// SHOOTER'S EDGE'S DEMOS PAGE (the main menu's DEMOS, `menu_demos`): the engine's folder browser
// (vfs/browse.hpp) over the player's own folders, drawn in the modern menu's look, to pick a
// recorded game anywhere on the machine (the game's folder, Downloads, a USB stick) and watch it.
//
// - WHAT IS LISTED: folders, and the files named as the game's recordings (the window hands the
//   game's extensions, `set_extensions`), plain or packed (`match.mvd.gz`, `duel.dem.zip`,
//   `final.mvd.7z`, `.tar.gz`); a packed name with no recording's extension under its packing is
//   left out. Newest first, so the file just downloaded is at the top.
// - THE PLACES: the game's own first (`set_game_places`: its folder), then the player's home,
//   Downloads, Desktop, Documents and the machine's roots; a row of them over the list, the one
//   the folder is in lit. It opens where the player last played a demo from (`menu_demo_dir`,
//   kept in the config), else the game's folder, else Downloads.
// - THE CHECK: ENTER on a file asks the window's check (`set_check`: the game reads the file,
// unpacks
//   it and reads it to its level's map, client/demo_check.hpp); a file that is no demo stays
//   listed with the reason in the footer and nothing starts. One that is gives `play` with its
//   path (`chosen`) and its facts line.
// - THE KEYS: UP and DOWN choose, ENTER (or a click) opens a folder or plays a demo, BACKSPACE (or
//   DELETE, or the `..` row) goes up a folder, LEFT and RIGHT (or a click on one) change the
//   place, R or F5 lists the folder again, ESCAPE goes back to the main menu.
//
// ge:summary Shooter's Edge's demos page: browse any folder for recordings, check one, play it
// ge:keywords demos page demo-browser menu_demos menu_demo_dir browse folder downloads places
// ge:keywords playdemo packed gz zip 7z check valid shooters-edge
// ge:skill ui shooter
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ui/draw.hpp"
#include "ui/menu.hpp"
#include "ui/menu_view.hpp"
#include "vfs/browse.hpp"

namespace ge::console {
class Console;
}

namespace shooters_edge::menu {

enum class DemosAction : uint8_t { none, leave, play };

class DemosPage {
 public:
  // Registers `menu_demo_dir` on `con`, which must outlive the page.
  void add_cvars(ge::console::Console& con);
  // The extensions listed (each with its dot): the game's recordings'.
  void set_extensions(std::vector<std::string> extensions) {
    browser_.set_extensions(std::move(extensions));
  }
  // The game's own places, put before the player's (its folder: "GAME").
  void set_game_places(std::vector<ge::vfs::Place> places) { game_places_ = std::move(places); }
  // The player's places in place of this machine's (`vfs::user_places`): a lab's or a test's
  // folders, the same on every machine. Empty: the machine's.
  void set_player_places(std::vector<ge::vfs::Place> places) { player_places_ = std::move(places); }
  // The window's check of a picked file: true with its facts line, false with why it is no demo.
  using Check = std::function<bool(const std::string& path, std::string& facts, std::string& why)>;
  void set_check(Check check) { check_ = std::move(check); }

  // The page opened: the places gathered, the folder last played from (or the first place that
  // opens) listed.
  void open();
  DemosAction input(const ge::ui::MenuInput& in);
  // The demo to play (after DemosAction::play), and what the check said of it.
  const std::string& chosen() const { return browser_.chosen(); }
  const std::string& facts() const { return facts_; }

  void draw(ge::ui::DrawList& list, int width, int height, const ge::ui::MenuStyle& style);
  const std::vector<std::string>& problems() const { return problems_; }

  ge::vfs::FolderBrowser& browser() { return browser_; }

 private:
  int hit_row(float x, float y) const;
  int hit_place(float x, float y) const;

  ge::console::Console* con_ = nullptr;
  ge::vfs::FolderBrowser browser_;
  std::vector<ge::vfs::Place> game_places_;
  std::vector<ge::vfs::Place> player_places_;
  Check check_;
  std::string last_dir_;  // menu_demo_dir
  std::string facts_;
  bool refused_ = false;             // the note is the check's refusal
  int first_ = 0;                    // the first row shown
  std::vector<ge::ui::Rect> cells_;  // the rows drawn last, from `first_`
  std::vector<ge::ui::Rect> chips_;  // the places drawn last
  std::vector<std::string> problems_;
};

}  // namespace shooters_edge::menu
