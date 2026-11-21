// TEAM CHAT AND THE GAME'S EVENTS IN A PLAYER'S CONFIG: what makes a teamplay config written for
// Quake's clients work here (docs/shooters-edge.md § Configs).
//
// - **say and say_team** (`add_team_chat`): the line's PERCENT CODES are filled in from the
//   player's own state, then its FUN CHARACTERS become the classic font's glyphs, then it is
//   quoted and sent. `cl_parseSay 0` and `cl_parseFunChars 0` turn either pass off.
// - **The percent codes** (`fill_team_codes`): `%h` health, `%a` armour, `%A` its class word,
//   `%c` cells, `%w` the weapon in hand and its ammunition (`rl:23`), `%b` the best weapon owned
//   by `tp_weapon_order` and its ammunition (in Quake II its own weapons by their words,
//   `tp_name_hb`, `rg`, `bfg` and kin, best first from the big gun down), `%p` and `%P` the
//   powerups held, `%R` the rune,
//   `%u` what the player needs (armour, health, the rocket launcher, the weapon's ammunition, by
//   the `tp_need_*` thresholds), `%l` the place the player is in, `%d` where they last died this
//   map, `%L` that place for five seconds after a death and the place now after, `%r` the last
//   place a code named, `%S` the skin. The bracketed forms `%[a]`, `%[h]` and `%[p]` put the
//   font's gold brackets round the figure when it is low (armour below 30, health below 50, any
//   powerup held). `%%` is a `%`; `%<width>X` with `l`, `r` or `c` before the `>` pads or cuts
//   (`%<5r>h`). `%i` the last pickup and where (`ra at quad`), `%X` the pickup, `%Y` its place,
//   all for `tp_tooktimeout` seconds; `%k` and `%m` the same for five. A pickup is read from the
//   state's change: a powerup's time starting, armour at a box's value, a new weapon, the big
//   health, a rune, rockets, cells or a pack. What the player points at (client/team_point.hpp):
//   `%t` the thing and its place (`quaded enemy at ra`), `%x` the thing, `%y` its place (the
//   player's own when nothing), `%j` the last thing pointed at within `tp_pointtimeout` seconds;
//   `nothing` when there is none. `%q` names the quad. The
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
//   `f_weaponchange` (the weapon in hand changed), `f_death` and `f_respawn` (health crossing 0),
//   `f_took` (a pickup) and `f_bonusflash` (the server's `bf`, flashed or not) run when the
//   event happens. `tp_triggers 0` stops them all; `f_death`, `f_respawn` and `f_took` are a
//   team's, run only with a teammate in the game or `tp_forceTriggers 1`. The game runs
//   `f_demostart`, `f_demoend` and `f_exit` through `team_event`; `cfg_load` runs `f_cfgload`
//   after the config it read (an alias the config defines included; not when there is no file).
// - **The canned messages** (`team_message`, the `tp_msg*` commands): `tp_msgreport`, `lost`,
//   `need`, `safe`, `killme`, `help`, `utake`, `enemypwr`, `quaddead`, `getquad`, `getpent` and the
//   one-line set (`yesok`, `nocancel`, `itemsoon`, `waiting`, `slipped`, `replace`, `trick`,
//   `coming`) say the player's state to the team behind their short name (`nick`, else the name's
//   first three letters, then `cl_fakename_suffix`); `tp_msgtook` the last pickup.
//   `tp_msgpoint` names what the player points at and where; `enemypwr`, `safe`, `killme`,
//   `utake`, `quaddead`, `getquad` and `getpent` read it too (a glowing enemy, a teammate).
// - **`echo`** and `set_ex` fill in the codes and fun characters (`set_ex2` codes only).
// The lines it prints are a player's: plain words, no next steps.
//
// ge:summary Team chat in a player's config: percent codes, fun characters, f_ event triggers
// ge:keywords say say_team percent-codes team-message fun-characters f_newmap f_spawn f_death
// ge:keywords f_respawn f_weaponchange tp_triggers tp_forcetriggers cl_parsesay
// ge:keywords cl_parsefunchars tp_name tp_need tp_weapon_order gold-brackets event-triggers
// ge:keywords tp_msgreport tp_msglost tp_msgneed tp_msgsafe canned-messages f_cfgload f_exit
// ge:keywords f_demostart f_demoend echo nick f_took tp_msgtook pickup took tp_tooktimeout
// ge:keywords tp_msgpoint tp_pointtimeout pointing set_ex set_ex2 set_tp macrolist f_bonusflash
// ge:skill shooter console
// ge:test tests/shooters_edge_team_chat_test.cpp
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "console/console.hpp"
#include "shooters_edge/client/place_names.hpp"
#include "shooters_edge/client/team_items.hpp"
#include "shooters_edge/client/team_point.hpp"
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
  int64_t bonuses = 0;           // the items taken the server told of, as last seen
  std::string died_at;           // where the player died this map ("": not yet)
  double died_t = -1e9;          // when, the state's clock
  mutable std::string reported;  // %r: the last place a code named
  std::string took;              // the last pickup's word setting ("tp_name_ra"; "": none)
  std::string took_at;           // where it was picked up
  double took_t = -1e9;          // when, the state's clock
  // What the player points at (client/team_point.hpp): this frame's scene (null: nothing in
  // sight), worked out on the frame's first pointing code; the last thing pointed at for `%j`.
  std::function<PointScene()> scene;
  mutable Pointed pointed;
  mutable double pointed_t = -1e9;  // the frame (the state's clock) `pointed` was worked out for
  mutable Pointed last_point;
  mutable double last_point_t = -1e9;
  TeamItems items;  // tp_took, tp_pickup, tp_point (client/team_items.hpp)
};

// The `say`/`say_team` commands and every setting the codes read, on `con` (`chat` outlives it).
void add_team_chat(ge::console::Console& con, TeamChat& chat);

// `text` with its percent codes filled in.
std::string fill_team_codes(const ge::console::Console& con, const TeamChat& chat,
                            std::string_view text);

// `text` with its fun characters made glyphs; `chat` false adds the `^c` rule.
std::string fun_characters(std::string_view text, bool chat);

// The canned message `tp_msg<which>` ("report", "safe", ...) as `say_team` sends it: the short
// name, then the body; "" when it says nothing.
std::string team_message(const ge::console::Console& con, const TeamChat& chat,
                         std::string_view which);

// The player's `alias` ("f_exit") queued, unless `tp_triggers` is 0.
void team_event(ge::console::Console& con, std::string_view alias);

// One frame of the player's state on `map` ("" when no level is up): kept for the codes, and
// the events it makes run. `bonuses` counts the items taken the server told of (its `bf`): one
// or more new ones run `f_bonusflash` once.
void team_frame(ge::console::Console& con, TeamChat& chat, const hud::Status& status,
                std::string_view map, int64_t bonuses = 0);

}  // namespace shooters_edge::client
