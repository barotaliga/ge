// Shooter's Edge's main menu: the table of entries, the pages built from it, the values read from
// the console and the events run as console lines (main_menu.hpp).
//
// ge:summary Builds the main menu's pages from its table, reads the cvars, runs the events' lines
// ge:keywords menu main-menu table entries refresh apply_event console-line dimmed new-game start
// ge:skill ui console
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/main_menu.hpp"

#include <cmath>

#include "console/cvar.hpp"

namespace shooters_edge::menu {

namespace {

using ge::ui::ItemKind;

constexpr Choice kGameType[] = {{"COOPERATIVE", "0"}, {"DEATHMATCH", "1"}};
constexpr Choice kTeamplay[] = {{"OFF", "0"}, {"NO FRIENDLY FIRE", "1"}, {"FRIENDLY FIRE", "2"}};
constexpr Choice kSkill[] = {{"EASY", "0"}, {"NORMAL", "1"}, {"HARD", "2"}, {"NIGHTMARE", "3"}};
constexpr Choice kFragLimit[] = {{"NONE", "0"}, {"10", "10"}, {"20", "20"},  {"30", "30"},
                                 {"40", "40"},  {"50", "50"}, {"60", "60"},  {"70", "70"},
                                 {"80", "80"},  {"90", "90"}, {"100", "100"}};
constexpr Choice kTimeLimit[] = {{"NONE", "0"},    {"5 MIN", "5"},   {"10 MIN", "10"},
                                 {"15 MIN", "15"}, {"20 MIN", "20"}, {"30 MIN", "30"},
                                 {"45 MIN", "45"}, {"60 MIN", "60"}};
constexpr Choice kMaxPlayers[] = {{"2", "2"}, {"4", "4"}, {"8", "8"}, {"16", "16"}};
constexpr Choice kColours[] = {{"0", "0"},   {"1", "1"},   {"2", "2"},   {"3", "3"},  {"4", "4"},
                               {"5", "5"},   {"6", "6"},   {"7", "7"},   {"8", "8"},  {"9", "9"},
                               {"10", "10"}, {"11", "11"}, {"12", "12"}, {"13", "13"}};
constexpr Choice kHudStyle[] = {
    {"CLASSIC", "classic"}, {"MODERN", "modern"}, {"COMPACT", "compact"}};
constexpr Choice kWorldLook[] = {{"CLASSIC", "classic"}, {"MODERN", "modern"}};
constexpr Choice kMenuLook[] = {{"AUTO", "auto"}, {"CLASSIC", "classic"}, {"MODERN", "modern"}};
constexpr Choice kOnOff[] = {{"OFF", "0"}, {"ON", "1"}};

// The menu, page by page. A row's `needs` dims it while the console knows no such name.
constexpr Entry kEntries[] = {
    // The root: the five rows of the classic main menu.
    {.page = "main",
     .id = "single",
     .text = "SINGLE PLAYER",
     .hint = "Play a map on your own.",
     .opens = "single"},
    {.page = "main",
     .id = "multi",
     .text = "MULTIPLAYER",
     .hint = "Start a match with its rules, or join one.",
     .opens = "multi"},
    {.page = "main",
     .id = "options",
     .text = "OPTIONS",
     .hint = "The mouse, the picture, the HUD and classic enhance.",
     .opens = "options"},
    {.page = "main", .id = "help", .text = "HELP", .hint = "The keys.", .opens = "help"},
    {.page = "main", .id = "quit", .text = "QUIT", .hint = "Leave the game.", .opens = "quit"},

    {.page = "single",
     .id = "new",
     .text = "NEW GAME",
     .hint = "The campaign from its first map, `start`. Needs a single-player program: the "
             "packs' own progs.dat.",
     .line = "newgame",
     .needs = "newgame",
     .closes = true},
    {.page = "single",
     .id = "load",
     .text = "LOAD",
     .hint = "Load a saved game: a slot of yours, or a classic .sav kept as s0 to s11.",
     .opens = "load",
     .needs = "load"},
    {.page = "single",
     .id = "save",
     .text = "SAVE",
     .hint = "Save the game into one of the slots.",
     .opens = "save",
     .needs = "save"},
    {.page = "single", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "multi",
     .id = "join",
     .text = "JOIN A GAME",
     .hint = "Find a server and connect to it. The client's network play comes with M18g.",
     .needs = "connect"},
    {.page = "multi",
     .id = "host",
     .text = "NEW GAME",
     .hint = "Start a match on this machine with its rules.",
     .opens = "host",
     .needs = "sv"},
    {.page = "multi",
     .id = "setup",
     .text = "SETUP",
     .hint = "Your name and colours.",
     .opens = "setup"},
    {.page = "multi", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "host",
     .id = "maxclients",
     .text = "MAX PLAYERS",
     .kind = ItemKind::choice,
     .hint = "How many may join. Others join once the client's network play is in.",
     .needs = "maxclients",
     .target = Target::server,
     .choices = kMaxPlayers},
    {.page = "host",
     .id = "deathmatch",
     .text = "GAME TYPE",
     .kind = ItemKind::choice,
     .hint = "Deathmatch, or the map played together.",
     .needs = "deathmatch",
     .target = Target::server,
     .choices = kGameType},
    {.page = "host",
     .id = "teamplay",
     .text = "TEAMPLAY",
     .kind = ItemKind::choice,
     .hint = "Teams by colour. The baseline mod has no teams yet.",
     .needs = "teamplay",
     .built = false,
     .target = Target::server,
     .choices = kTeamplay},
    {.page = "host",
     .id = "skill",
     .text = "SKILL",
     .kind = ItemKind::choice,
     .hint = "How hard the monsters fight. The baseline mod has no skill levels yet.",
     .needs = "skill",
     .built = false,
     .target = Target::server,
     .choices = kSkill},
    {.page = "host",
     .id = "fraglimit",
     .text = "FRAG LIMIT",
     .kind = ItemKind::choice,
     .hint = "The match ends when a player reaches it.",
     .needs = "fraglimit",
     .target = Target::server,
     .choices = kFragLimit},
    {.page = "host",
     .id = "timelimit",
     .text = "TIME LIMIT",
     .kind = ItemKind::choice,
     .hint = "The match ends when the time is up.",
     .needs = "timelimit",
     .target = Target::server,
     .choices = kTimeLimit},
    {.page = "host",
     .id = "map",
     .text = "MAP",
     .kind = ItemKind::choice,
     .hint = "The map the match starts on.",
     .needs = "map"},
    {.page = "host",
     .id = "start",
     .text = "START",
     .hint = "Start the match with these rules.",
     .needs = "map",
     .closes = true},
    {.page = "host", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "setup",
     .id = "name",
     .text = "NAME",
     .hint = "Your name, from the next map. Type `name <yours>` in the console.",
     .line = "toggleconsole",
     .needs = "name",
     .closes = true},
    {.page = "setup",
     .id = "topcolor",
     .text = "SHIRT COLOUR",
     .kind = ItemKind::choice,
     .hint = "Your shirt's colour. Player models are not drawn yet.",
     .needs = "topcolor",
     .target = Target::client,
     .choices = kColours},
    {.page = "setup",
     .id = "bottomcolor",
     .text = "PANTS COLOUR",
     .kind = ItemKind::choice,
     .hint = "Your pants' colour. Player models are not drawn yet.",
     .needs = "bottomcolor",
     .target = Target::client,
     .choices = kColours},
    {.page = "setup", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "options",
     .id = "controls",
     .text = "CUSTOMIZE CONTROLS",
     .hint = "Bind the keys. The game's moves are not on binds yet.",
     .needs = "+forward"},
    {.page = "options",
     .id = "console",
     .text = "GO TO CONSOLE",
     .hint = "Drop the console.",
     .line = "toggleconsole",
     .needs = "toggleconsole",
     .closes = true},
    {.page = "options",
     .id = "reset",
     .text = "RESET TO DEFAULTS",
     .hint = "Every setting on these pages back to its default."},
    {.page = "options",
     .id = "viewsize",
     .text = "SCREEN SIZE",
     .kind = ItemKind::slider,
     .hint = "The view's size.",
     .needs = "viewsize",
     .target = Target::client,
     .min = 30,
     .max = 120,
     .step = 10},
    {.page = "options",
     .id = "gamma",
     .text = "BRIGHTNESS",
     .kind = ItemKind::slider,
     .hint = "The picture's brightness.",
     .needs = "gamma",
     .target = Target::client,
     .min = 0.5,
     .max = 1,
     .step = 0.05},
    {.page = "options",
     .id = "sensitivity",
     .text = "MOUSE SPEED",
     .kind = ItemKind::slider,
     .hint = "Degrees of turn a mouse count.",
     .needs = "sensitivity",
     .target = Target::client,
     .min = 0.011,
     .max = 0.22,
     .step = 0.011},
    {.page = "options",
     .id = "bgmvolume",
     .text = "MUSIC VOLUME",
     .kind = ItemKind::slider,
     .hint = "The game has no music.",
     .needs = "bgmvolume",
     .target = Target::client,
     .min = 0,
     .max = 1,
     .step = 0.1},
    {.page = "options",
     .id = "volume",
     .text = "SOUND VOLUME",
     .kind = ItemKind::slider,
     .hint = "The game's sound. The window plays no sound yet.",
     .needs = "volume",
     .target = Target::client,
     .min = 0,
     .max = 1,
     .step = 0.1},
    {.page = "options",
     .id = "cl_run",
     .text = "ALWAYS RUN",
     .kind = ItemKind::toggle,
     .hint = "Run without holding the run key.",
     .needs = "cl_run",
     .target = Target::client},
    {.page = "options",
     .id = "m_invert",
     .text = "INVERT MOUSE",
     .kind = ItemKind::toggle,
     .hint = "The mouse's pitch upside down.",
     .needs = "m_invert",
     .target = Target::client},
    {.page = "options",
     .id = "lookspring",
     .text = "LOOKSPRING",
     .kind = ItemKind::toggle,
     .hint = "The view centres when you move.",
     .needs = "lookspring",
     .target = Target::client},
    {.page = "options",
     .id = "lookstrafe",
     .text = "LOOKSTRAFE",
     .kind = ItemKind::toggle,
     .hint = "The mouse strafes while the look key is held.",
     .needs = "lookstrafe",
     .target = Target::client},
    {.page = "options",
     .id = "video",
     .text = "VIDEO OPTIONS",
     .hint = "The world's look, the HUD and the menu's look.",
     .opens = "video"},
    {.page = "options",
     .id = "classic",
     .text = "CLASSIC ENHANCE",
     .hint = "Your own classic packs enhanced, and every enhancement on or off.",
     .needs = "ce_textures"},
    {.page = "options", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "video",
     .id = "r_look",
     .text = "WORLD LOOK",
     .kind = ItemKind::choice,
     .hint = "The world as the classic game drew it, or the modern picture.",
     .needs = "r_look",
     .target = Target::client,
     .choices = kWorldLook},
    {.page = "video",
     .id = "hud_style",
     .text = "HUD",
     .kind = ItemKind::choice,
     .hint = "The classic status bar, the modern corners or the compact rings.",
     .needs = "hud_style",
     .target = Target::client,
     .choices = kHudStyle},
    {.page = "video",
     .id = "menu_look",
     .text = "MENU LOOK",
     .kind = ItemKind::choice,
     .hint = "AUTO draws this menu in the pack's own pictures when the pack has them.",
     .needs = "menu_look",
     .target = Target::client,
     .choices = kMenuLook},
    {.page = "video",
     .id = "vid_fullscreen",
     .text = "FULLSCREEN",
     .kind = ItemKind::choice,
     .hint = "The window over the whole screen.",
     .needs = "vid_fullscreen",
     .target = Target::client,
     .choices = kOnOff},
    {.page = "video", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "help",
     .id = "keys",
     .text = "W A S D  MOVE     SPACE  JUMP",
     .kind = ItemKind::label},
    {.page = "help",
     .id = "fire",
     .text = "MOUSE  LOOK     LEFT BUTTON  FIRE",
     .kind = ItemKind::label},
    {.page = "help",
     .id = "weapons",
     .text = "1 - 8  WEAPON     WHEEL  NEXT WEAPON",
     .kind = ItemKind::label},
    {.page = "help",
     .id = "console",
     .text = "THE KEY UNDER ESCAPE  CONSOLE",
     .kind = ItemKind::label},
    {.page = "help", .id = "menu", .text = "ESCAPE  THIS MENU", .kind = ItemKind::label},
    {.page = "help", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "quit", .id = "ask", .text = "LEAVE SHOOTER'S EDGE?", .kind = ItemKind::label},
    {.page = "quit",
     .id = "yes",
     .text = "YES",
     .hint = "Quit the game.",
     .line = "quit",
     .needs = "quit",
     .closes = true},
    {.page = "quit", .id = "no", .text = "NO", .kind = ItemKind::back},
};

constexpr const char* kPages[][2] = {
    {"main", ""},       {"single", "SINGLE PLAYER"}, {"multi", "MULTIPLAYER"}, {"host", "NEW GAME"},
    {"setup", "SETUP"}, {"options", "OPTIONS"},      {"video", "VIDEO"},       {"help", "HELP"},
    {"quit", "QUIT"},
};

// The slot pages, built here rather than from the table: one row a slot, its words the save's
// line (`Host::saves`), and the slot's console line `load s<i>` or `save s<i>`.
constexpr const char* kSlotPages[][2] = {{"load", "LOAD"}, {"save", "SAVE"}};

bool slot_page(std::string_view page) { return page == "load" || page == "save"; }

std::string slot_id(int i) { return "s" + std::to_string(i); }

bool same_value(std::string_view a, std::string_view b) {
  double x = 0, y = 0;
  if (ge::console::parse_number(a, x) && ge::console::parse_number(b, y)) return x == y;
  return ge::console::lower(a) == ge::console::lower(b);
}

bool shown(const Entry& e, const Host& host) {
  return e.built && (e.needs.empty() || host_knows(host, e.needs));
}

// The value a setting writes from its item now.
std::string item_text(const Entry& e, const ge::ui::Item& it) {
  switch (e.kind) {
    case ItemKind::toggle:
      return it.on ? "1" : "0";
    case ItemKind::choice:
      if (it.choice >= 0 && static_cast<size_t>(it.choice) < e.choices.size())
        return std::string(e.choices[static_cast<size_t>(it.choice)].value);
      return {};
    case ItemKind::slider:
      return ge::console::number_text(it.value);
    default:
      return {};
  }
}

}  // namespace

std::span<const Entry> entries() { return kEntries; }

const Entry* find_entry(std::string_view page, std::string_view id) {
  for (const Entry& e : kEntries)
    if (e.page == page && e.id == id) return &e;
  return nullptr;
}

bool host_knows(const Host& host, std::string_view name) {
  if (host.console != nullptr &&
      (host.console->cvars().find(name) != nullptr || host.console->has_command(name)))
    return true;
  return host.server != nullptr && host.server->find(name) != nullptr;
}

std::string host_value(const Host& host, std::string_view name) {
  if (host.console != nullptr)
    if (const ge::console::Cvar* c = host.console->cvars().find(name)) return c->value;
  if (host.server != nullptr)
    if (const ge::console::Cvar* c = host.server->find(name)) return c->value;
  return {};
}

ge::ui::Menu build_menu() {
  ge::ui::Menu m;
  for (const auto& [id, title] : kPages) {
    ge::ui::Page& page = m.add_page(id, title);
    for (const Entry& e : kEntries) {
      if (e.page != id) continue;
      ge::ui::Item it{.id = std::string(e.id), .text = std::string(e.text), .kind = e.kind};
      it.hint = std::string(e.hint);
      it.opens = std::string(e.opens);
      it.prominent = e.page == "host" && e.id == "start";
      for (const Choice& c : e.choices) it.choices.emplace_back(c.text);
      it.min = e.min, it.max = e.max, it.step = e.step, it.unit = std::string(e.unit);
      page.items.push_back(std::move(it));
    }
  }
  for (const auto& [id, title] : kSlotPages) {
    ge::ui::Page& page = m.add_page(id, title);
    for (int i = 0; i < kSaveSlots; i++) {
      ge::ui::Item it{.id = slot_id(i), .text = std::string(kEmptySlot)};
      it.hint = std::string(id) == "load" ? "Load this game." : "Save the game here.";
      page.items.push_back(std::move(it));
    }
    page.items.push_back({.id = "back", .text = "BACK", .kind = ItemKind::back});
  }
  return m;
}

void refresh(ge::ui::Menu& menu, const Host& host, bool hide_back) {
  for (const auto& [id, title] : kPages) {
    ge::ui::Page* page = menu.page(id);
    if (page == nullptr) continue;
    for (ge::ui::Item& it : page->items) {
      const Entry* e = find_entry(id, it.id);
      if (e == nullptr) continue;
      if (e->kind == ItemKind::back) {
        it.disabled = hide_back && e->page != "quit";
        continue;
      }
      if (e->kind == ItemKind::label) continue;
      it.disabled = !shown(*e, host);
      if (e->page == "host" && e->id == "map") {
        const std::string was =
            it.choices.empty() ? std::string() : it.choices[static_cast<size_t>(it.choice)];
        it.choices.clear();
        for (const std::string& map : host.maps) it.choices.push_back(map);
        if (it.choices.empty()) {
          it.choices.push_back("NONE");
          it.disabled = true;
        }
        it.choice = 0;
        for (size_t i = 0; i < it.choices.size(); i++)
          if (it.choices[i] == was) it.choice = static_cast<int>(i);
        continue;
      }
      if (e->id == "name") {
        const std::string name = host_value(host, "name");
        it.text = name.empty() ? std::string(e->text) : std::string(e->text) + "  " + name;
        continue;
      }
      if (e->target == Target::none || it.disabled) continue;
      const std::string v = host_value(host, e->id);
      if (e->kind == ItemKind::toggle) {
        double x = 0;
        it.on = ge::console::parse_number(v, x) && x != 0;
      } else if (e->kind == ItemKind::choice) {
        for (size_t i = 0; i < e->choices.size(); i++)
          if (same_value(v, e->choices[i].value)) it.choice = static_cast<int>(i);
      } else if (e->kind == ItemKind::slider) {
        double x = 0;
        if (ge::console::parse_number(v, x)) it.value = std::fmin(e->max, std::fmax(e->min, x));
      }
    }
    if (page->focus >= 0 && !page->items[static_cast<size_t>(page->focus)].stop())
      page->focus = -1;  // the menu moves it to the first stop
  }
  for (const auto& [id, title] : kSlotPages) {
    ge::ui::Page* page = menu.page(id);
    if (page == nullptr) continue;
    const bool load = std::string_view(id) == "load";
    const bool known = host_knows(host, id);
    for (ge::ui::Item& it : page->items) {
      if (it.kind == ItemKind::back) {
        it.disabled = hide_back;
        continue;
      }
      const size_t i = std::stoul(it.id.substr(1));
      const std::string line = i < host.saves.size() ? host.saves[i] : std::string();
      it.text = line.empty() ? std::string(kEmptySlot) : line.substr(0, kSlotChars);
      it.disabled = !known || (load && line.empty());
    }
    if (page->focus >= 0 && !page->items[static_cast<size_t>(page->focus)].stop()) page->focus = -1;
  }
}

Outcome apply_event(const ge::ui::MenuEvent& e, ge::ui::Menu& menu, Host& host) {
  if (e.kind == ge::ui::EventKind::back_at_root) return Outcome::back_at_root;
  if (host.console == nullptr) return Outcome::none;
  ge::console::Console& con = *host.console;
  if (slot_page(e.page)) {
    if (e.kind != ge::ui::EventKind::activated || e.item.empty() || e.item[0] != 's')
      return Outcome::none;
    con.execute(e.page + " " + e.item);
    return Outcome::close;
  }
  const Entry* entry = find_entry(e.page, e.item);
  ge::ui::Page* page = menu.page(e.page);
  if (entry == nullptr || page == nullptr) return Outcome::none;
  const ge::ui::Item* it = page->find(e.item);
  if (it == nullptr) return Outcome::none;
  if (e.kind == ge::ui::EventKind::changed) {
    if (entry->target == Target::client)
      con.execute(std::string(entry->id) + " " + item_text(*entry, *it));
    return Outcome::none;
  }
  if (e.kind != ge::ui::EventKind::activated) return Outcome::none;
  if (e.page == "options" && e.item == "classic") return Outcome::classic_enhance;
  if (e.page == "options" && e.item == "reset") {
    reset_defaults(host);
    refresh(menu, host);
    return Outcome::none;
  }
  if (e.page == "host" && e.item == "start") {
    for (const ge::ui::Item& row : page->items) {
      const Entry* r = find_entry("host", row.id);
      if (r == nullptr || r->target != Target::server || row.disabled) continue;
      con.execute("sv " + std::string(r->id) + " " + item_text(*r, row));
    }
    const ge::ui::Item* map = page->find("map");
    if (map != nullptr && !map->disabled && !map->choices.empty())
      con.execute("map " + map->choices[static_cast<size_t>(map->choice)]);
    return Outcome::close;
  }
  if (!entry->line.empty()) con.execute(entry->line);
  return entry->closes ? Outcome::close : Outcome::none;
}

void reset_defaults(Host& host) {
  if (host.console == nullptr) return;
  for (const Entry& e : kEntries) {
    if (e.target != Target::client) continue;
    if (ge::console::Cvar* c = host.console->cvars().find(e.id))
      host.console->cvars().reset(*c, ge::console::Source::local);
  }
}

}  // namespace shooters_edge::menu
