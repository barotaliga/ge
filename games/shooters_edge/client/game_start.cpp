// ge:summary A game started from SELECT GAME: the console down with the loading lines, then up
// ge:keywords game-start scr_conspeed con_speed
// ge:skill console shooter
// ge:test tests/shooters_edge_game_start_test.cpp
#include "shooters_edge/client/game_start.hpp"
#include "shooters_edge/client/cvar_layers.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

#include "core/num_text.hpp"

namespace shooters_edge::client {

namespace {

double number(std::string_view text, double fallback) {
  // strtod, not from_chars: Apple's library has no from_chars for a double.
  const std::string s(text);
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  return end != s.c_str() ? v : fallback;
}

}  // namespace

void add_start_cvars(ge::console::Console& c, GameStart& s, const profiles::GameProfile& game) {
  ge::console::CvarLayer shell(c.cvars(), kLayerShell);  // client/cvar_layers.hpp
  s.per_screen = game.conspeed_screen;
  s.own_default = game.conspeed;
  c.cvars().bind(
      {.name = "scr_conspeed",
       .fallback = std::string(game.conspeed),
       .flags = ge::console::kArchive,
       .help = "How fast the console slides up and down.",
       .kind = ge::console::CvarKind::number,
       .min = 0,
       .max = 100000,
       .values = "0: at once; NUMBER: the game's own speed (300 in Quake, 3 in Quake II)"},
      &s.conspeed);
  // Whether the game's default.cfg runs, at start-up and when another game is loaded
  // (client/install_configs.cpp); off, a player's own configs alone set the game up.
  c.cvars().add({.name = "fs_default",
                 .fallback = "1",
                 .flags = ge::console::kArchive,
                 .help = "Whether the game's default.cfg runs at start-up and when another game is "
                         "loaded, before your own settings.",
                 .kind = ge::console::CvarKind::boolean,
                 .values = "0: your own configs alone (at start-up only from the command line); "
                           "1: it runs (the default)"});
}

void start_switch_game(ge::console::Console& c, GameStart& s, const profiles::GameProfile& was,
                       const profiles::GameProfile& now) {
  s.per_screen = now.conspeed_screen;
  s.own_default = now.conspeed;
  if (s.conspeed == number(was.conspeed, s.conspeed))
    c.cvars().set("scr_conspeed", now.conspeed, ge::console::Source::code);
  s.applied = -1;
}

void start_apply_speed(ge::console::Console& c, GameStart& s) {
  if (s.conspeed == s.applied) return;
  // Another game's default (a config written while it was loaded): the loaded game's own.
  const double own = number(s.own_default, s.conspeed);
  for (const profiles::GameProfile& g : profiles::kGameProfiles)
    if (const double theirs = number(g.conspeed, own); theirs != own && s.conspeed == theirs) {
      c.cvars().set("scr_conspeed", s.own_default, ge::console::Source::code);
      break;
    }
  s.applied = s.conspeed;
  const double screens = s.per_screen > 0 ? s.conspeed / s.per_screen : s.conspeed;
  c.cvars().set("con_speed", ge::core::num::to_string(screens), ge::console::Source::code);
}

void start_begin(GameStart& s, ge::console::ConsoleView& v, ge::console::Console& c, double now_s,
                 const std::vector<std::string>& lines) {
  s.loading = true;
  s.since = now_s;
  v.set_open(false);
  v.set_full(true);
  for (const std::string& l : lines) c.print(l);
}

bool start_step(GameStart& s, ge::console::ConsoleView& v, double now_s, bool up) {
  if (!s.loading) return false;
  const double held = now_s - s.since;
  if (!(up && held >= kStartHold) && held < kStartMost) return false;
  s.loading = false;
  v.set_full(false);
  return true;
}

std::vector<std::string> start_pack_lines(const std::vector<std::string>& packs,
                                          const std::string& basedir) {
  std::vector<std::string> lines;
  lines.reserve(packs.size());
  for (const std::string& pack : packs)
    lines.push_back("Added packfile " +
                    std::filesystem::path(pack).lexically_relative(basedir).generic_string());
  return lines;
}

}  // namespace shooters_edge::client
