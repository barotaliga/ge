// THE TEAM'S `$` MACROS: the player's state as words a config's line uses anywhere (`echo`, `if`,
// a bind, `say_team`), the names QuakeWorld's later client gave them, in every classic game
// (porting/shooters-edge/teamplay-scripting.md holds the facts). Each reads what team chat's
// percent codes read (team_chat.hpp), in the team's own words (`tp_name_*`), so a Quake II
// player's `$bestweapon` is `rg` and a Quake player's `rl`.
//
// - **Figures**: `$health`, `$armor`, `$shells`, `$nails`, `$rockets`, `$cells`, `$ammo` (the
//   weapon in hand's), `$weaponnum` (1..8; Quake II's key, 1..11).
// - **Words**: `$armortype` (`tp_name_armortype_*`, `tp_name_none` with none), `$weapon` (in hand),
//   `$weapons` (every one held, best first, joined by `tp_name_separator`), `$bestweapon` and
//   `$bestammo` (by `tp_weapon_order`), `$powerups` (`tp_name_none` with none),
//   `$colored_powerups` (the same with no separator), `$colored_short_powerups` (`q`, `r`, `p`),
//   `$tp_powerups` (one of the two by `tp_poweruptextstyle`), `$colored_armor` (the armour figure,
//   `0` with none), `$need` (`%u`).
// - **Places** (place_names.hpp): `$location` (`%l`), `$deathloc` (`%d`), `$lastloc` (`%L`),
//   `$took` (`%X`), `$tookloc` (`%Y`), `$tookatloc` (`%i`), `$point` (`%x`), `$pointloc` (`%y`),
//   `$pointatloc` (`%j`).
// - **Lights**: `$ledpoint` (`tp_name_status_red` an enemy pointed at, `_green` a teammate,
//   `_yellow` a powerup, `_blue` anything else or nothing) and `$ledstatus` (green with nothing
//   needed, yellow one thing, red two or more; the rocket launcher not counted).
// - **The match** (a server joined or a recording watched): `$matchstatus` (`disconnected`,
//   `standby` or `normal`), `$team1` and `$team2` (the players' first two teams by name, their
//   names with teamplay off; `team1`, `team2` when there are not so many).
// Every word a light names is a fun character by default (`$G`, `$R`, `$Y`, `$B`), made a glyph
// where the line's fun characters are.
//
// ge:summary The team's $ macros: health, armour, weapons, powerups, places and lights as words
// ge:keywords health armor armortype shells nails rockets cells ammo weapon weaponnum weapons
// ge:keywords bestweapon bestammo powerups colored_powerups colored_short_powerups tp_powerups
// ge:keywords colored_armor need location deathloc lastloc took tookloc tookatloc point pointloc
// ge:keywords pointatloc ledpoint ledstatus tp_name_status tp_name_flag team-macros dollar-macros
// ge:keywords matchstatus team1 team2
// ge:skill shooter console
// ge:test tests/shooters_edge_team_chat_test.cpp
#pragma once

#include <string>

#include "console/console.hpp"
#include "shooters_edge/client/team_chat.hpp"

namespace shooters_edge::client {

// The macros above on `con`, reading `chat` (both outlive it); add_team_chat calls it.
void add_team_macros(ge::console::Console& con, TeamChat& chat);

}  // namespace shooters_edge::client
