// TEAM CHAT AND THE GAME'S EVENTS IN A PLAYER'S CONFIG: what makes a teamplay config written for
// the classic game's clients work here (docs/shooters-edge.md § Configs).
//
// - **say and say_team** (`add_team_chat`): the line's PERCENT CODES are filled in from the
//   player's own state, then its FUN CHARACTERS become the classic font's glyphs, then it is
//   quoted and sent. `cl_parseSay 0` and `cl_parseFunChars 0` turn either pass off.
// - **The percent codes** (`fill_team_codes`): `%h` health, `%a` armour, `%A` its class word,
//   `%c` cells, `%w` the weapon in hand and its ammunition (`rl:23`), `%b` the best weapon owned
//   by `tp_weapon_order` and its ammunition, `%p` and `%P` the powerups held, `%R` the rune,
//   `%u` what the player needs (armour, health, the rocket launcher, the weapon's ammunition, by
//   the `tp_need_*` thresholds), `%l` the place the player is in, `%d` where they last died this
//   map, `%L` that place for five seconds after a death and the place now after, `%r` the last
//   place a code named, `%S` the skin. The bracketed forms `%[a]`, `%[h]` and `%[p]` put the
//   font's gold brackets round the figure when it is low (armour below 30, health below 50, any
//   powerup held). `%%` is a `%`; `%<width>X` with `l`, `r` or `c` before the `>` pads or cuts
//   (`%<5r>h`). What the player took or points at is not followed yet: `%i %j %k %m %t %x %X %y
//   %Y %q` say `nothing` (and `%y` the player's own place), as with nothing taken or seen. The
//   player counts (`%e %E %o %O %s`) need a server's leave and stay as written. Any other letter
//   is left as written. Every word is a `tp_name_*` setting, plain words by default (`rl`, `ga`,
//   `quad`, `nothing`, `at`), so a player's own words stand.
// - **Fun characters** (`fun_characters`): `$[` `$]` the gold brackets, `$0`..`$9` the gold
//   digits, `$G $R $Y $B $W` the five lights, `$( $= $) $a` the slider, `$b` a box, `$c $d $\`
//   the arrowhead, `$<` `$-` `$>` the separator bar, `$,` `$.` dots, `$:` a new line, `$$` and
//   `$^` themselves, `$xHH` any byte (`$x00` a blank). Outside chat (`chat` false) `^c` puts the
//   next character in the font's coloured half. An unknown pair is left as written.
// - **The events** (`team_frame`, once a frame with the player's state and the map): aliases a
//   player names `f_newmap` (a new map, and every connect or demo), `f_spawn` (its first frame),
//   `f_weaponchange` (the weapon in hand changed), `f_death` and `f_respawn` (health crossing 0)
//   run when the event happens. `tp_triggers 0` stops them all; `f_death` and `f_respawn` are a
//   team's, run only with a teammate in the game or `tp_forceTriggers 1`.
//
// The lines it prints are a player's: plain words, no next steps.
//
// ge:summary Team chat in a player's config: percent codes, fun characters, f_ event triggers
// ge:keywords say say_team percent-codes team-message fun-characters f_newmap f_spawn f_death
// ge:keywords f_respawn f_weaponchange tp_triggers tp_forcetriggers cl_parsesay
// ge:keywords cl_parsefunchars tp_name tp_need tp_weapon_order gold-brackets event-triggers
// ge:skill shooter console
// ge:test tests/shooters_edge_team_chat_test.cpp
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "console/console.hpp"
#include "shooters_edge/client/place_names.hpp"
#include "shooters_edge/hud/status.hpp"

namespace shooters_edge::client {

struct TeamChat {
  const PlaceNames* places = nullptr;  // %l and the other places (null: someplace)
  // The line `say "..."` or `say_team "..."` sent; false when no game is playing.
  std::function<bool(const std::string& line)> send;
  // Whether a teammate is in the game (null: never), for the team's own events.
  std::function<bool()> teammates;

  // What the codes read, kept by `team_frame`.
  hud::Status status;
  bool known = false;            // a frame's state has come
  std::string map;               // the map the events were run for
  int last_health = 0;           // the health the last frame had
  int last_weapon = -1;          // the weapon in hand (-1: none yet this map)
  std::string died_at;           // where the player died this map ("": not yet)
  double died_t = -1e9;          // when, the state's clock
  mutable std::string reported;  // %r: the last place a code named
};

// The `say`/`say_team` commands and every setting the codes read, on `con` (`chat` outlives it).
void add_team_chat(ge::console::Console& con, TeamChat& chat);

// `text` with its percent codes filled in.
std::string fill_team_codes(const ge::console::Console& con, const TeamChat& chat,
                            std::string_view text);

// `text` with its fun characters made glyphs; `chat` false adds the `^c` rule.
std::string fun_characters(std::string_view text, bool chat);

// One frame of the player's state on `map` ("" when no level is up): kept for the codes, and
// the events it makes run.
void team_frame(ge::console::Console& con, TeamChat& chat, const hud::Status& status,
                std::string_view map);

}  // namespace shooters_edge::client
