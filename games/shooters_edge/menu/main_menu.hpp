// SHOOTER'S EDGE'S MAIN MENU AS A MODEL: the pages the ESCAPE key opens over the game, laid out
// the way Quake's menu is (SINGLE PLAYER, MULTIPLAYER, OPTIONS, HELP, QUIT and the
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
//   line and says whether the menu closes. CLASSIC ENHANCED ▸ LOAD GAME and ENHANCE, the help pages
//   and CUSTOMIZE CONTROLS' waiting for a key are the game menu's (game_menu.hpp).
// - **CLASSIC ENHANCED** starts with LOAD GAME (the game's install, lit by the console's `basedir`)
//   and ENHANCE, then holds the modern picture's settings a player will want to change, page by
//   subject (textures and models; light and shadow; water, lava and sky; fire and wind; weather):
//   the `ce_` switches, the `r_` cvars and the weather's; the rest stay console-only.
//
// ge:summary Shooter's Edge's main menu model: the classic pages, entries wired to cvars, dimming
// ge:keywords menu main-menu escape single-player multiplayer options help quit new-game setup
// ge:keywords video dimmed disabled cvar console-line needs togglemenu shooters-edge
// ge:keywords load-page save-page save-slots customize-controls controls bind_control bound_keys
// ge:keywords classic-enhanced ce_ graphics weather held-weapon gamma brightness load-game
// ge:keywords screenshots gallery demos demo-browser select-game ranked-game matchmaking
// ge:skill ui console
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "server/sheet.hpp"
#include "shooters_edge/menu/controls_page.hpp"
#include "ui/menu.hpp"
#include "vfs/search.hpp"

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
  int bit = -1;           // toggle: the one bit of the cvar's number it reads and writes (-1 none)
  bool inverted = false;  // toggle: on writes 0 (or clears the bit)
  std::string_view bind = {};  // a key row: the command a key is bound to (shown as its keys)
  // A choice of video modes: `choices` (one: the windowed row) then each mode of the cvar this
  // names (`vid_modelist`, words apart), and the choice's number written (0 windowed, N mode N).
  std::string_view modes_from = {};
  // A text setting (`needs`): its value shown beside its words, typed in the console.
  bool field = false;
};

// A page of a menu: its id and its title ("" for the root, which shows the game's own picture).
struct PageTitle {
  std::string_view id;
  std::string_view title;
};

// A GAME'S MAIN MENU: its pages in order and every page's rows. Each classic game has its own (its
// adapter hands it out: client/game_adapter.hpp's `menu`), so a game shows its own pages, its own
// modes (a game with no single-player campaign has no SINGLE PLAYER) and its own words.
// Quake's is `classic_menu()`.
struct MenuTable {
  std::span<const PageTitle> pages;
  std::span<const Entry> entries;
  std::string_view empty_slot;      // the words of an empty save slot on LOAD and SAVE
  std::string_view in_level = {};   // the page ESCAPE opens in a level ("" the root)
  std::string_view script = {};     // the menu script the game reads over it (menu_script.hpp)
  const MenuTable* over = nullptr;  // a script's table: the game's table it was read over
  // The saves LOAD and SAVE list, a row each, in order ("" a gap); empty: Quake's `s0` to `s11`
  // on both (slots_of).
  std::span<const std::string_view> load_slots = {};
  std::span<const std::string_view> save_slots = {};
};
// The game's own table `t` is, or was read over.
inline const MenuTable& game_table(const MenuTable& t) { return t.over != nullptr ? *t.over : t; }
// Quake's (an empty slot "--- UNUSED SLOT ---", as the original's), and the same pages for
// Quake II with our own words for an empty slot.
const MenuTable& classic_menu();
const MenuTable& q2_menu();

// QUAKE'S QUIT BOX: one of its eight jokes, picked at random each time the box opens and kept while
// it stays open, four lines of 24 characters as the game shows them (a blank last line spaces).
// Y quits, N or ESCAPE goes back; any other letter is ignored.
inline constexpr size_t kQuitJokes = 8;
inline constexpr size_t kQuitJokeLines = 4;
std::span<const std::string_view, kQuitJokeLines> quit_joke(size_t i);

// The QUALITY row's levels, 0 to 10 (quality.hpp); both games' VIDEO pages show it, and CUSTOM
// beside them when the settings make none (main_menu.cpp's `quality_row`).
inline constexpr Choice kQualityChoices[] = {
    {"0 CLASSIC", "0"}, {"1", "1"}, {"2", "2"}, {"3", "3"}, {"4", "4"},  {"5", "5"},
    {"6", "6"},         {"7", "7"}, {"8", "8"}, {"9", "9"}, {"10", "10"}};

// Quake's table, every page's rows in order.
std::span<const Entry> entries();
// The entry of `page` with `id` in `table`; null when none.
const Entry* find_entry(std::string_view page, std::string_view id,
                        const MenuTable& table = classic_menu());

// What the menu reads and drives: the client's console (its cvars, its commands, the lines run),
// the listen server's cvars (null when no server runs), the maps NEW GAME offers.
struct Host {
  ge::console::Console* console = nullptr;
  const ge::console::Cvars* server = nullptr;
  std::vector<std::string> maps = {};  // map names, `start` first when the packs hold it
  // Each save's line, in the order of `slot_names(table)`: its title, "" for an empty slot.
  std::vector<std::string> saves = {};
  const MenuTable* table = nullptr;  // the game's menu (null: classic_menu())
  bool in_level = false;             // a level is being played (ESCAPE opens `in_level`)
  // In a level, a screen of the rules is up (Quake II's help computer, scoreboard or inventory):
  // ESCAPE sends `putaway` to take it down and opens no menu; held on (`escape_held`, ESCAPE down
  // this frame) to its first repeat, it opens the in-game page after all.
  bool screen_up = false;
  bool escape_held = false;
  // The game's files (null: none): the player page's figures and skins (figures.hpp).
  const ge::vfs::SearchPath* files = nullptr;
  // The server settings pages' sheet (net/qw_remote_sheet.hpp; null: none): what OPTIONS ▸ SERVER
  // SETTINGS shows (server_page.hpp).
  const ge::server::SettingsSheet* sheet = nullptr;
};

// The command a key row binds: a CUSTOMIZE CONTROLS row's move or an entry's `bind`; "" none.
std::string_view bind_command(const MenuTable& table, std::string_view page, std::string_view item);

// The table `host` reads.
const MenuTable& table_of(const Host& host);

// Quake's save slots (`s0` to `s11`, on LOAD and SAVE alike), and the words of an empty one.
inline constexpr int kSaveSlots = 12;
// The saves the LOAD (`load`) or SAVE page of `table`'s game lists, in order ("" a gap).
std::span<const std::string_view> slots_of(const MenuTable& table, bool load);
// Every save either page lists, once each, LOAD's first: what `Host::saves` follows.
std::vector<std::string> slot_names(const MenuTable& table);
inline constexpr std::string_view kEmptySlot = "- EMPTY -";
inline constexpr std::string_view kUnusedSlot = "--- UNUSED SLOT ---";  // Quake's
// A slot's line cut to what a row holds: the classic look's label column, and the modern look's
// slab on a phone (the title shortened before the kills are).
inline constexpr size_t kSlotChars = 22;
inline constexpr size_t kSlotCharsModern = 28;

// Whether `name` is a cvar or a command of the console or a cvar of the server.
bool host_knows(const Host& host, std::string_view name);
// A cvar's value as text, the client's first; "" when neither has it.
std::string host_value(const Host& host, std::string_view name);

// The pages: `main` (the root: SELECT GAME, SINGLE PLAYER, MULTIPLAYER, DEMOS, OPTIONS, HELP,
// QUIT), `single`, `multi`, `host` (NEW GAME), `setup`, `ranked` (RANKED GAME: a duel looked for,
// the console for a two-on-two's duo, the queue left), `options`, `video` and its `weapon`,
// `enhanced` (CLASSIC ENHANCED) and its `ce_art`, `ce_light`, `ce_liquids`, `ce_fire` and
// `ce_weather`, `help`, `quit`; the slot pages `load` and `save` (a row a save, its id the save's
// name: Quake's `s0`..`s11`, Quake II's autosave `save0`, a gap, `save1`..`save15` on LOAD and
// `save1`..`save15` on SAVE; then BACK; a slot activated runs `load <name>` or `save <name>` and
// closes the menu; an empty slot is dimmed on LOAD); and `controls` (CUSTOMIZE CONTROLS: a row
// `c<i>` a move of `controls()`, its value the keys bound to it).
ge::ui::Menu build_menu(const MenuTable& table = classic_menu());

// Every entry's dimming and value read from the host (on opening, and after the console changed
// something); the NEW GAME map list rebuilt from `host.maps`. `hide_back`: the classic look, which
// has no BACK rows (Escape goes back), dims them so the focus skips them.
void refresh(ge::ui::Menu& menu, const Host& host, bool hide_back = false);

// How many of TEAMPLAY's words the host page offers with the packs' `maps`: off, no friendly fire
// and friendly fire; with Dissolution of Eternity's maps (r1m1) its tag, capture the flag, one flag
// and three team games too (teamplay 3 to 6).
size_t teamplay_choices(std::span<const std::string> maps);

// What an event asks of the game menu.
// `bind_key`: a CUSTOMIZE CONTROLS row was pressed; the game menu waits for the key (the event's
// item names the row). `load_game`: LOAD GAME, the window shows the desktop's folder chooser.
// `classic_enhance`: ENHANCE, the enhancing watched. `screenshots`: OPTIONS ▸ SCREENSHOTS, the
// gallery's page shown (gallery_page.hpp). `servers`: JOIN A GAME's browser. `demos`: DEMOS, the
// demos page (demos_page.hpp). `games`: SELECT GAME, the games page (games_page.hpp).
enum class Outcome : uint8_t {
  none,
  close,
  classic_enhance,
  load_game,
  back_at_root,
  bind_key,
  screenshots,
  servers,
  demos,
  games
};

// One menu event applied: a client setting's line run, NEW GAME's START running the rules and the
// map, an action's line run. Returns whether the menu closes or opens something it owns.
Outcome apply_event(const ge::ui::MenuEvent& e, ge::ui::Menu& menu, Host& host);

// Each client setting the menu shows put back to its default, and every key to the game's layout
// where the console has `bind_defaults` (OPTIONS ▸ RESET TO DEFAULTS).
void reset_defaults(Host& host);

}  // namespace shooters_edge::menu
