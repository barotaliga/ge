// TEAM CHAT AND THE GAME'S EVENTS IN A PLAYER'S CONFIG (docs/shooters-edge.md § Configs).
// - **say and say_team** (`add_team_chat`): the line's PERCENT CODES are filled in from the
//   player's own state, then its FUN CHARACTERS become the classic font's glyphs, then it is quoted
//   and sent. `cl_parseSay 0` and `cl_parseFunChars 0` turn either pass off. With `cl_fakename`
//   set, a team line starts with a carriage return, that name and `cl_fakename_suffix`, drawn over
//   the `(name): ` the others' clients write (the canned messages then put no short name of their
//   own). `cl_floodprot 1` keeps back a line past `cl_fp_messages` (4, at most 10) in any
//   `cl_fp_persecond` (4) seconds, saying "Flood Protection".
// - **The percent codes** (`fill_team_codes`): `%h` health, `%a` armour, `%A` its class word, `%c`
//   cells, `%w` the weapon in hand and its ammunition (`rl:23`), `%b` the best weapon owned by
//   `tp_weapon_order` and its ammunition (Quake II's by its own words, `rg`, `bfg`, best first),
//   `%p` `%P` the powerups, `%R` the rune, `%u` what the player needs (`tp_need_*`), `%l` the place
//   the player is in, `%d` where they last died this map, `%L` that place for five seconds after a
//   death and the place now after, `%r` the last place named, `%S` the skin. `%[a]`, `%[h]`, `%[p]`
//   add gold brackets when low (armour under 30, health under 50, a powerup held). `%%` is a `%`;
//   `%<width>X` with `l`, `r` or `c` before the `>` pads or cuts
//   (`%<5r>h`). `%i` the last pickup and where (`ra at quad`), `%X` the pickup, `%Y` its place, all
//   for `tp_tooktimeout` seconds; `%k` and `%m` the same for five. What the player points at
//   (client/team_point.hpp): `%t` the thing and its place (`quaded enemy at ra`), `%x` the thing,
//   `%y` its place (the player's own when nothing), `%j` the last thing pointed at within
//   `tp_pointtimeout` seconds; `nothing` when there is none. `%q` names the quad. The player counts
//   (`%e %E %o %O %s`) need a server's leave and stay as written. Other letters stay as written.
//   Every word is a `tp_name_*` setting (`rl`, `ga`, `quad`, `at`).
// - **Fun characters** (`fun_characters`): `$[` `$]` the gold brackets, `$0`..`$9` the gold digits,
//   `$G $R $Y $B $W` the five lights, the slider, box, arrowhead, bar and dot glyphs, `$:` a new
//   line, `$xHH` any byte. Outside chat (`chat` false) `^c` puts the next character in the font's
//   coloured half. An unknown pair is left as written.
// - **The events** (`team_frame`, once a frame with the player's state and the map): aliases a
//   player names `f_newmap` (a new map, and every connect or demo), `f_spawn` (its first frame),
//   `f_weaponchange` (the weapon in hand changed), `f_death` and `f_respawn` (health crossing 0),
//   `f_took` (a pickup) and `f_bonusflash` run when the event happens. On a server joined or a
//   recording watched also `f_mapend` (the level's end), `f_countdownstart` (the server's `status`
//   turned to countdown), `f_countdownbreak` (back to standby from it) and `f_flagdeath` in
//   `f_death`'s place when the player died carrying a flag (`carries_flag`). `tp_triggers 0` stops
//   them all; the deaths, `f_respawn` and `f_took` are a team's, run only with a teammate in the
//   game or `tp_forceTriggers 1`. The game runs `f_demostart`, `f_demoend`, `f_demomatchstart`,
//   `f_focusgained` and `f_exit` through `team_event`, the server list `f_sbrefreshdone`, and the
//   spectator's camera `f_trackspectate` and `f_freeflyspectate` (`spectate_event`, no gate);
//   `cfg_load` runs `f_cfgload` after the config it read.
// - **The canned messages** (`team_message`, the `tp_msg*` commands): `tp_msgreport`, `lost`,
//   `need`, `safe`, `killme`, `help`, `utake`, `enemypwr`, `quaddead`, `getquad`, `getpent` and the
//   one-line set (`yesok`, `nocancel`, `itemsoon`, `waiting`, `slipped`, `replace`, `trick`,
//   `coming`) say the player's state to the team behind their short name (`nick`, else the name's
//   first three letters, then `cl_fakename_suffix`); `tp_msgtook` the last pickup. `tp_msgpoint`
//   names what the player points at; seven others read it too.
// - **`echo`** and `set_ex` fill in the codes and fun characters (`set_ex2` codes only).
// - **The `$` macros** (`$health`, `$location`...) are team_macros.hpp's, added here.
// The lines it prints are a player's: plain words, no next steps.
// ge:summary Team chat in a player's config: percent codes, fun characters, f_ event triggers
// ge:keywords say say_team percent-codes team-message fun-characters f_newmap f_spawn f_death
// ge:keywords f_respawn f_weaponchange tp_triggers tp_forcetriggers cl_parsesay cl_parsefunchars
// ge:keywords tp_name tp_need tp_weapon_order gold-brackets event-triggers tp_msgreport tp_msglost
// ge:keywords tp_msgneed tp_msgsafe canned-messages f_cfgload f_exit f_demostart f_demoend echo
// ge:keywords nick f_took tp_msgtook pickup took tp_tooktimeout tp_msgpoint tp_pointtimeout
// ge:keywords pointing set_ex set_ex2 set_tp macrolist f_bonusflash cl_fakename cl_floodprot
// ge:keywords cl_fp_messages cl_fp_persecond flood-protection f_mapend f_countdownstart
// ge:keywords f_countdownbreak f_flagdeath f_focusgained f_trackspectate f_freeflyspectate
// ge:keywords f_demomatchstart f_sbrefreshdone carries-flag
// ge:skill shooter console
// ge:test tests/shooters_edge_team_chat_test.cpp
#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

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
  double level_from = 0;         // the state's clock when that map's first frame came ($cl_timer)
  int last_health = 0;           // the health the last frame had
  int last_weapon = -1;          // the weapon in hand (-1: none yet this map)
  int64_t bonuses = 0;           // the items taken the server told of, as last seen
  bool flag = false;             // the player carried a flag (or a key) in the last frame
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
  // What the server joined or the recording watched said last (team_frame's `state`), for
  // $matchstatus, $team1, $team2 and the match's events.
  bool joined = false;        // a frame came with a state ($matchstatus "disconnected" else)
  bool intermission = false;  // its level has ended
  std::string match_status;   // the server's `status` ("countdown", "standby", ...)
  std::string team1, team2;   // the first two teams by name (with teamplay off, players)
  // When the last lines were sent (cl_floodprot), a ring of ten.
  std::array<double, 10> sent_at{-1e9, -1e9, -1e9, -1e9, -1e9, -1e9, -1e9, -1e9, -1e9, -1e9};
  int sent_next = 0;
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

// How many things `s` needs by the `tp_need_*` thresholds (what %u lists), the rocket launcher
// counted only with `rl`.
int needed_count(const ge::console::Console& con, const hud::Status& s, bool rl);
// The weapons `s` holds, best first, in the team's words (Quake II's by its keys); the melee
// weapon last.
std::vector<std::string> held_weapon_words(const ge::console::Console& con, const hud::Status& s);
// The weapon in hand in the team's words, and its number (1..8; Quake II's key) in `number`.
std::string hand_weapon_word(const ge::console::Console& con, const hud::Status& s, int& number);

// The player's `alias` ("f_exit") queued, unless `tp_triggers` is 0.
void team_event(ge::console::Console& con, std::string_view alias);

// The player's `alias` queued whenever it exists, `tp_triggers` or not: the spectator's camera
// locked on a player (`f_trackspectate`) or let go of one (`f_freeflyspectate`).
void spectate_event(ge::console::Console& con, std::string_view alias);

// Whether the player `state` is of carries a flag: a capture-the-flag flag's glow on him (Quake
// and QuakeWorld's two flag effects, Quake II's two) or, in Team Fortress, a key among his items.
bool carries_flag(const DemoState& state);

// One frame of the player's state on `map` ("" when no level is up): kept for the codes, and
// the events it makes run. `bonuses` counts the items taken the server told of (its `bf`): one
// or more new ones run `f_bonusflash` once.
void team_frame(ge::console::Console& con, TeamChat& chat, const hud::Status& status,
                std::string_view map, int64_t bonuses = 0, const DemoState* state = nullptr);

}  // namespace shooters_edge::client
