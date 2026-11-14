// ge:summary A game started from SELECT GAME: the console down with the loading lines, then up
// ge:keywords game-start scr_conspeed con_speed
// ge:skill console shooter
// ge:test tests/shooters_edge_game_start_test.cpp
#include "shooters_edge/client/game_start.hpp"

#include <charconv>

#include "core/num_text.hpp"

namespace shooters_edge::client {

namespace {

double number(std::string_view text, double fallback) {
  double v = fallback;
  const auto r = std::from_chars(text.data(), text.data() + text.size(), v);
  return r.ec == std::errc{} ? v : fallback;
}

}  // namespace

void add_start_cvars(ge::console::Console& c, GameStart& s, const profiles::GameProfile& game) {
  s.per_screen = game.conspeed_screen;
  c.cvars().bind(
      {.name = "scr_conspeed",
       .fallback = std::string(game.conspeed),
       .flags = ge::console::kArchive,
       .help = "How fast the console slides up and down.",
       .kind = ge::console::CvarKind::number,
       .min = 0,
       .max = 100000,
       .values =
           "0: at once; NUMBER: the game's own speed (300 in the classic game, 3 in the sequel)"},
      &s.conspeed);
}

void start_switch_game(ge::console::Console& c, GameStart& s, const profiles::GameProfile& was,
                       const profiles::GameProfile& now) {
  s.per_screen = now.conspeed_screen;
  if (s.conspeed == number(was.conspeed, s.conspeed))
    c.cvars().set("scr_conspeed", now.conspeed, ge::console::Source::code);
  s.applied = -1;
}

void start_apply_speed(ge::console::Console& c, GameStart& s) {
  if (s.conspeed == s.applied) return;
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

}  // namespace shooters_edge::client
