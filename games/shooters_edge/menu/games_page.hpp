// SHOOTER'S EDGE'S SELECT GAME PAGE (the main menu's first row, `menu_games`): the classic games
// the shell plays, each named by a picture of its name (`menu/art/<profile id>.png`, compiled in;
// the names are drawn, never written), with the install of it last loaded on this computer and
// which edition that is (client/installs.hpp), the game playing now marked.
//
// - ENTER on a game: the game playing now goes back to the main menu; one whose install was loaded
//   before opens it at once (`GamesAction::open`, `chosen().install`); one never loaded asks the
//   window for the desktop's folder chooser (`GamesAction::browse`) to find the folder holding its
//   base folder and first pack. Whichever game that folder turns out to be is the one loaded.
// - FIND ANOTHER INSTALL (the last row) asks for the folder chooser for any game, or another copy
//   of one already found.
// - A folder that held no game the shell knows comes back as a note (`set_note`) in the footer.
// - UP and DOWN choose, ENTER (or a click) picks, ESCAPE goes back to the main menu.
//
// Drawn in the modern menu's look (cards, the name's picture large) or, once a classic game is
// loaded, in its classic look (classic_pages.hpp: its plaque, its glyph sheet).
//
// ge:summary The SELECT GAME page: each classic game by its name's picture, its install, edition
// ge:keywords menu select-game games-page game-names name-picture install edition shareware
// ge:keywords registered browse find-install menu_games
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/menu/classic_art.hpp"
#include "ui/draw.hpp"
#include "ui/image.hpp"
#include "ui/menu.hpp"
#include "ui/menu_view.hpp"

namespace shooters_edge::menu {

// One game the page offers.
struct GameRow {
  std::string id;        // its content profile's id (classicenhance/profile.hpp)
  std::string install;   // the folder of it loaded last on this computer, "" none
  std::string edition;   // that install's edition in the game's words, "" unknown
  bool playing = false;  // the game loaded now
  bool missing = false;  // `install` was kept but no longer opens
};

enum class GamesAction : uint8_t { none, leave, open, browse };

// The picture of the game `id`'s name, white on clear, its letters bevelled; empty for an id
// with none.
const ge::ui::Image& game_name_picture(std::string_view id);

class GamesPage {
 public:
  // What the window knows of the games, asked each time the page opens.
  using Lister = std::function<std::vector<GameRow>()>;
  void set_lister(Lister lister) { lister_ = std::move(lister); }
  // The rows in place of the lister's (a lab's, a test's).
  void set_games(std::vector<GameRow> rows) { rows_ = std::move(rows); }
  const std::vector<GameRow>& games() const { return rows_; }

  // The page opened: the rows asked for, the focus on the game playing (else the first).
  void open();
  GamesAction input(const ge::ui::MenuInput& in);
  // The game picked (after open or browse; browse from the last row: an empty id).
  const GameRow& chosen() const { return chosen_; }
  // Why the last folder picked did not load ("" clears it).
  void set_note(std::string note) { note_ = std::move(note); }
  const std::string& note() const { return note_; }
  int selected() const { return selected_; }

  void draw(ge::ui::DrawList& list, int width, int height, const ge::ui::MenuStyle& style);
  void draw_classic(ge::ui::DrawList& list, const ClassicArt& art, int width, int height, double t);
  const std::vector<std::string>& problems() const { return problems_; }

 private:
  int hit(float x, float y) const;
  int count() const { return static_cast<int>(rows_.size()) + 1; }  // the games, FIND ANOTHER

  Lister lister_;
  std::vector<GameRow> rows_;
  GameRow chosen_;
  std::string note_;
  int selected_ = 0;
  std::vector<ge::ui::Rect> cells_;
  std::vector<std::string> problems_;
};

}  // namespace shooters_edge::menu
