// SHOOTER'S EDGE'S MAIN MENU AS A MODEL: the pages the ESCAPE key opens over the game, laid out
// the way the classic game's menu is (SINGLE PLAYER, MULTIPLAYER, OPTIONS, HELP, QUIT and the
// pages under them), on the engine's menu model (ui/menu.hpp). It holds no pixels: the modern look
// is the engine's menu view, the classic look is classic_view.hpp, and both draw this model.
//
// - **Every entry is wired to the console.** A setting is a cvar (the client's, or the listen
//   server's for NEW GAME's rules) read when the menu opens and written by a console line when the
//   player changes it, so the menu, the console and a config are one setting. An action is a
//   console line (`map start`, `toggleconsole`, `quit`).
// - **What the game cannot do yet is shown DIMMED** (shown, skipped by the focus). An entry names
//   what it needs (`Entry::needs`: a cvar or a command); it is dimmed while the console knows no
//   such name, so it lights up by itself the day the cvar or command is registered. An entry the
//   game has the name for but does nothing with (`Entry::built` false: the baseline mod reads no
//   `skill`) is dimmed with the reason in its hint.
// - **Events** (`apply_event`): a setting changed runs its line at once (the client's) or waits
//   for START (the server's rules, run as `sv <cvar> <value>` before the map); an action runs its
//   line and says whether the menu closes. CLASSIC ENHANCED ▸ ENHANCE A PACK, the help pages and
//   CUSTOMIZE CONTROLS' waiting for a key are the game menu's (game_menu.hpp).
// - **CLASSIC ENHANCED** holds the modern picture's settings a player will want to change, page by
//   subject (textures and models; light and shadow; water, lava and sky; fire and wind; weather):
//   the `ce_` switches, the `r_` cvars and the weather's; the rest stay console-only.
//
// ge:summary Shooter's Edge's main menu model: the classic pages, entries wired to cvars, dimming
// ge:keywords menu main-menu escape single-player multiplayer options help quit new-game setup
// ge:keywords video dimmed disabled cvar console-line needs togglemenu shooters-edge
// ge:keywords load-page save-page save-slots customize-controls controls bind_control bound_keys
// ge:keywords classic-enhanced ce_ graphics weather held-weapon gamma brightness
// ge:skill ui console
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "shooters_edge/menu/controls_page.hpp"
#include "ui/menu.hpp"

namespace shooters_edge::menu {

// What a setting writes.
enum class Target : uint8_t {
  none,    // an action or a heading: nothing stored
  client,  // a client cvar, set at once (`<cvar> <value>`)
  server,  // a listen server cvar, set by START (`sv <cvar> <value>`)
};

// One choice of a choice entry: the words shown and the value written.
struct Choice {
  std::string_view text = {};
  std::string_view value = {};
};

// One row of the menu's table (main_menu.cpp, `kEntries`); its fields stay in the order the
// table's designated initializers read them.
struct Entry {                 // NOLINT(clang-analyzer-optin.performance.Padding)
  std::string_view page = {};  // the page it is on
  std::string_view id = {};    // the item's id; a setting's is its cvar
  std::string_view text = {};  // the words
  ge::ui::ItemKind kind = ge::ui::ItemKind::action;
  std::string_view hint = {};
  std::string_view opens = {};  // action: the page it opens
  std::string_view line = {};   // action: the console line it runs
  std::string_view needs = {};  // a cvar or command the console must know, or "" (always there)
  bool built = true;            // false: dimmed even when `needs` is known (the hint says why)
  bool closes = false;          // action: the menu closes after the line runs
  bool reversed =
      false;  // slider: shown 0..100 % from max (left) to min (right), as gamma brightens
  Target target = Target::none;
  std::span<const Choice> choices = {};  // choice
  double min = 0, max = 1, step = 0.1;   // slider
  std::string_view unit = {};            // slider
};

// The table, every page's rows in order.
std::span<const Entry> entries();
// The entry of `page` with `id`; null when none.
const Entry* find_entry(std::string_view page, std::string_view id);

// What the menu reads and drives: the client's console (its cvars, its commands, the lines run),
// the listen server's cvars (null when no server runs), the maps NEW GAME offers.
struct Host {
  ge::console::Console* console = nullptr;
  const ge::console::Cvars* server = nullptr;
  std::vector<std::string> maps = {};  // map names, `start` first when the packs hold it
  // Each save slot's line (slot i is the save `s<i>`): the save's title, "" for an empty slot.
  std::vector<std::string> saves = {};
};

// The save slots the LOAD and SAVE pages show, and the words of an empty one.
inline constexpr int kSaveSlots = 12;
inline constexpr std::string_view kEmptySlot = "- EMPTY -";
// A slot's line cut to what a row holds: the classic look's label column, and the modern look's
// slab on a phone (the title shortened before the kills are).
inline constexpr size_t kSlotChars = 22;
inline constexpr size_t kSlotCharsModern = 28;

// Whether `name` is a cvar or a command of the console or a cvar of the server.
bool host_knows(const Host& host, std::string_view name);
// A cvar's value as text, the client's first; "" when neither has it.
std::string host_value(const Host& host, std::string_view name);

// The pages: `main` (the root), `single`, `multi`, `host` (NEW GAME), `setup`, `options`,
// `video` and its `weapon`, `enhanced` (CLASSIC ENHANCED) and its `ce_art`, `ce_light`,
// `ce_liquids`, `ce_fire` and `ce_weather`, `help`, `quit`; the slot pages `load` and `save` (rows
// `s0`..`s11`, then BACK; a slot activated runs `load s<i>` or `save s<i>` and closes the menu; an
// empty slot is dimmed on LOAD); and `controls` (CUSTOMIZE CONTROLS: a row `c<i>` a move of
// `controls()`, its value the keys bound to it).
ge::ui::Menu build_menu();

// Every entry's dimming and value read from the host (on opening, and after the console changed
// something); the NEW GAME map list rebuilt from `host.maps`. `hide_back`: the classic look, which
// has no BACK rows (Escape goes back), dims them so the focus skips them.
void refresh(ge::ui::Menu& menu, const Host& host, bool hide_back = false);

// What an event asks of the game menu.
// `bind_key`: a CUSTOMIZE CONTROLS row was pressed; the game menu waits for the key (the event's
// item names the row).
enum class Outcome : uint8_t { none, close, classic_enhance, back_at_root, bind_key };

// One menu event applied: a client setting's line run, NEW GAME's START running the rules and the
// map, an action's line run. Returns whether the menu closes or opens something it owns.
Outcome apply_event(const ge::ui::MenuEvent& e, ge::ui::Menu& menu, Host& host);

// Each client setting the menu shows put back to its default (OPTIONS ▸ RESET TO DEFAULTS).
void reset_defaults(Host& host);

}  // namespace shooters_edge::menu
