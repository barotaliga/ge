// A GAME'S MENU SCRIPT: pages and rows written as text, read over the game's own menu table
// (main_menu.hpp). Quake II's is `q2pro.menu` on the game's search path (`MenuTable::script`), so
// a player or a mod changes the menu by editing a file.
//
// - **The file**: one command a line; `//` and `/* */` comments outside quotes; a backslash at a
//   line's end joins the next line; words split like the console's, quotes grouping. Each line's
//   `$name` is expanded when it is read (never inside quotes, so a quoted command keeps its `$` for
//   when it runs); `$$` is a `$`.
// - **Top level**: `include <file>` (16 deep), `begin <page>` ... `end`, and the look's words
//   (`background`, `font`, `cursor`, `weapon`, `color`), read and checked; ours draws its own look.
// - **A page's words**: `title`, `banner`, `plaque`, `background`, `style`, and the rows: `values`
//   (the setting holds the choice's number), `strings` (its words), `pairs` (shown, stored),
//   `range` (a slider; min above max runs it the other way), `toggle` (`~` inverted, a bit
//   number for one bit), `bind`, `action` (a `pushmenu` to a page opens it), `bitmap`, `savegame`,
//   `loadgame`, `blank`, `field` (a text setting, typed in the console). `-s` / `--status` is a
//   row's hint. In `values` and `strings`, an item `$$name` becomes the setting's words, one item a
//   word.
// - **Over the game's table**: a script page replaces the game's page of the same name (Quake II's
//   names for ours: `singleplayer`, `multiplayer`, `startserver`, `addressbook`), a new page is
//   added; the pages the game draws itself (player setup, load, save, the browsers, controls) stay
//   its own, and the root keeps SELECT GAME first. A row whose setting or command the console does
//   not know is dimmed, as every row is.
// - **Words for the player**: a line the script cannot use is told in the console with the file
//   and the line, and what was done (the line skipped, the row or the page left out, the rest of
//   the file not read).
//
// ge:summary A menu script read over a game's menu table: pages and rows as text, $ expansion
// ge:keywords menu-script q2pro-menu pages rows include begin end values strings pairs range
// ge:keywords toggle bind action bitmap savegame loadgame blank field pushmenu menu_script
// ge:keywords set_table
// ge:skill ui console
// ge:test tests/shooters_edge_menu_script_test.cpp
#pragma once

#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/menu/main_menu.hpp"

namespace shooters_edge::menu {

// A file the script includes, by its name on the search path; nullopt when there is none.
using ScriptFiles = std::function<std::optional<std::string>(std::string_view name)>;
// A name's value for `$name` (a cvar or a macro); false when there is none.
using ScriptLookup = std::function<bool(std::string_view name, std::string& value)>;

// A menu script read over a game's table. Its table's views point into it: not copied or moved.
class ScriptMenu {
 public:
  ScriptMenu() = default;
  ScriptMenu(const ScriptMenu&) = delete;
  ScriptMenu& operator=(const ScriptMenu&) = delete;

  // `text` (the file `name`) read over `base`; what it could not use is in `warnings`.
  void read(std::string_view name, std::string_view text, const MenuTable& base,
            const ScriptFiles& files, const ScriptLookup& lookup);
  // The game's table with the script's pages laid over it (`base` until `read`).
  const MenuTable& table() const { return table_; }
  // The pages the script defined.
  int pages() const { return pages_read_; }
  // Each line the script could not use, in the player's words.
  const std::vector<std::string>& warnings() const { return warnings_; }

 private:
  struct Reader;
  std::string_view keep(std::string s);

  std::deque<std::string> text_;             // every string a view points into
  std::deque<std::vector<Choice>> choices_;  // every row's choices
  std::vector<PageTitle> page_list_;
  std::vector<Entry> rows_;
  MenuTable table_{};
  int pages_read_ = 0;
  std::vector<std::string> warnings_;
};

// The game's page a script's page name stands for (`singleplayer` → `single`); the name itself
// when ours has none.
std::string_view script_page(std::string_view name);

}  // namespace shooters_edge::menu
