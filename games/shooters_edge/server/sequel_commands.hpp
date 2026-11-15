// THE SEQUEL'S PLAYER COMMANDS: the lines a player's client sends its rules between moves (typed
// in the console, or bound to a key by the install's own config), as the game answers them.
//
// - **Every line is a command or chat**: a word the rules do not know is said to everyone, the
//   word and the rest of the line, as `say` would say it.
// - **The intermission** takes only `players`, `say`, `say_team`, `score` and `help`; the rest,
//   and the chat of an unknown word, are ignored.
// - **Chat**: `say` to everyone, `say_team` to the team when `dmflags` 64 or 128 makes teams
//   (else to everyone), seen as `Name: text` and `(Name): text`, cut at 150 characters. A text in
//   double quotes loses them. Flood protection: past `flood_msgs` lines in `flood_persecond`
//   seconds the player is quiet for `flood_waitdelay` seconds, and told so.
// - **The cheats** (refused in deathmatch unless `cheats` is 1): `god` (no blow harms him, the
//   push still lands; a telefrag still kills), `notarget` (the monsters do not take him up as an
//   enemy; a fight under way goes on), `noclip` (the spectator's flight through walls, no water,
//   no triggers, no pickups; a respawn walks again) and `give`: `all`, `health [n]`, `weapons`,
//   `ammo`, `armor`, or an item by its name (ammunition set to the count after it, or added as a
//   pickup; anything else taken as a pickup, by the pickup's rules). An item `dmflags` leaves out
//   is not given.
// - **`weaplast`**: the weapon held before the last switch, as `use` would take it up.
// - **`wave <n>`**: a gesture (0 flip off, 1 salute, 2 taunt, 3 wave, 4 and past it point),
//   standing on the ground with nothing above a gesture playing (sequel_poses.hpp).
// - **`players`**: the players by frags, lowest first, then their count; **`playerlist`**: each
//   in slot order with his time in the match, his ping and his score.
// - **`use` and `drop`** first say why an item cannot be: no such item, not one to use or drop,
//   none held; then sequel_fight.hpp's rules. `kill` clears `god`.
//
// The facts: porting/shooters-edge/sequel-rules.md § The player's commands in detail.
//
// ge:summary The sequel's player commands: chat and its flood guard, the cheats, give, gestures
// ge:keywords sequel client-command say say_team chat flood-protection cheats give god notarget
// ge:keywords noclip weaplast wave gesture players playerlist unknown-command intermission
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_commands_test.cpp
#pragma once

#include <string>

namespace shooters_edge::server {

struct SequelServer;

// The settings the commands read: `cheats`, `flood_msgs`, `flood_persecond`, `flood_waitdelay`.
void commands_cvars(SequelServer& s);

// A player (re)spawned: noclip off, the flood record cleared, the weapon in hand his last.
void commands_spawn(SequelServer& s, int slot);

// The frame's end: each player's last weapon follows a completed switch.
void commands_frame(SequelServer& s);

// Player `slot`'s command `line` (its first word already known not to be the screens', `kill`'s
// or the fight's own); false when the line is empty. An unknown word is chat.
bool commands_run(SequelServer& s, int slot, const std::string& line);

// Whether the intermission lets command `cmd` run.
bool commands_in_intermission(const std::string& cmd);

// `use` or `drop` of `name` refused before the fight's rules (no such item, not usable or not
// droppable, none held): true when refused, the reason printed to the player.
bool commands_refuse_item(SequelServer& s, int slot, bool use, const std::string& name);

// The item row `item` given to player `slot` as if he touched it (sequel_server.cpp): the
// pickup's rules in full; false when refused or left out by `dmflags`.
bool sq_give(SequelServer& s, int slot, int item);

}  // namespace shooters_edge::server
