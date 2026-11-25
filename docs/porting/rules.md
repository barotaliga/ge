# Rules and mods: the runner, our rules in C++, community mods, campaign, co-op, saves

What a game IS once its files are open: its rules (items, weapons, damage, movers, monsters,
deathmatch, the campaign), the runner a host steps them through, the community's mods in whatever
form they shipped, the engine's own C++ mods stacked on any game's rules, co-op and its
companions, and saves.

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/mod/` (`abi.hpp`, `loader.hpp`, `manifest.hpp`, `signature.hpp`, `wasm/`) | C++ mods: the C ABI, the package and its manifest, the loader; the store form as a WebAssembly module (`docs/mods.md`) |
| Engine | `engine/save/` | The save file: chunks, the state described once (`docs/saves.md`) |
| Shooter kit | `plugins/shooter/program.hpp`, `entity.hpp`, `physics.hpp`, `push.hpp`, `area.hpp` | The game-program seam (hooks by function number, the globals the loop sets), the entity pool, the physics frame, pushers, the area tree |
| Shooter kit | `plugins/shooter/rules_world.hpp`, `bot_view.hpp` | The rules' world every game's rules fill (lists, clients, rows); what a bot sees of it |
| Progs plugin | `plugins/progs/vm.hpp`, `file.hpp`, `layout.hpp`, `extensions.hpp`, `stdlib.hpp`, `qc_strings.hpp` | THE QC VM for community mods' bytecode: loader and checker, the layouts by program form, the extension registry, builtins |
| Progs plugin | `plugins/progs/qvm.hpp` | The compiled-module VM: a mod shipped as a module, run sandboxed |
| Progs plugin | `plugins/progs/native.hpp`, `qc_compile.hpp`, `tool.hpp` | A native program (a progs whose functions are C++); the QC compiler for tests and fixtures; `ge qc` |
| Classic plugin | `plugins/classic/item_tables.hpp`, `item_roles.hpp`, `coop.hpp` | The item tables' shapes; items as shared roles (remix lays one game's as another's); the co-op settings both games share |
| Shell | `games/shooters_edge/server/rules_runner.hpp` | THE RUNNER: `RunnerCalls`, a table a program form fills, and `RulesCommon` every runner keeps; C++ mods laid over any runner |
| Shell | `games/shooters_edge/server/qc_server.hpp`, `catalogue.hpp`, `builtins.hpp`, `rules_world.hpp` | The QC runner: the full builtin catalogue, `checkextension`, the rules' world filled |
| Shell | `games/shooters_edge/server/qvm_server.hpp` | The compiled-module runner |
| Shell | `games/shooters_edge/rules/` (`arena.hpp`, `campaign.hpp`, `calls.hpp`) | OUR OWN RULES in C++ as native programs: the arena's deathmatch and the campaign, over the classic server's calls typed for C++ |
| Shell | `games/shooters_edge/server/campaign.hpp`, `campaign_episodes.hpp`, `campaign_save.hpp`, `q1_save.hpp` | The campaign host: maps in a chain, episodes, saves on `engine/save`, the classic text save |
| Shell | `games/shooters_edge/sim/mod_api.hpp`, `mod_host.hpp` | A C++ mod's hooks and services over the program seam |
| Shell | `games/shooters_edge/profiles/arsenal.hpp`, `items.hpp`, `words.hpp` | A game's weapons and armour, items and server words as data rows |
| Shell | `plugins/shooterbots/companion.hpp`, `client/coop_mode.hpp`, `client/coop_party.hpp` | Co-op: companions, the mode, the party carried map to map |

## Each game's

| | Quake | Quake II | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Our rules | `rules/arena.hpp`, `rules/campaign.hpp` (C++ native programs) | `server/q2_server.hpp` and its `q2_*` files (items, combat, fight, map, targets, monsters, poses, campaign, screens, gibs, save) | its arena rules in C++ on a runner | its rules in C++ |
| Community mods | QC bytecode on the VM (`qc_server.hpp`); compiled modules (`qvm_server.hpp`) | its native game library hosted unchanged (`server/q2_native.hpp`) | its compiled modules on the module VM; native libraries | none (its mods are data) |
| C++ mods | the program seam (`sim/mod_host.hpp`) | laid over its own entities (`server/q2_mods.hpp`) | the seam | the seam |
| Saves | `campaign_save.hpp`, the classic text save | `server/q2_save.hpp`; a game library's own by its files (`server/q2_native_save.cpp`) | | |

## Adding a game

1. A reader's facts on the rules (`porting/shooters-edge/<game>-rules.md`): items, weapons' numbers
   and timing, damage, movers, monsters, the campaign. Numbers come from the game's own program and
   files, never typed from memory.
2. The rules in C++ on a runner: a new `RunnerCalls` table over `RulesCommon` when the game keeps
   its own entities (Quake II's), else the program seam. NEVER QC of our own.
3. Data a rule needs from another game's rules is header-only data (a roster, a pose table) so
   either rules library reads it without linking the other's code.
4. Its items as roles (`item_roles.hpp`), its arsenal and words as rows (`profiles/`).
5. The community's mod form: a runner for it if the VM exists (the module VM does), else named
   and passed over (`runs_form`), never read as another form.
6. C++ mods laid over its rules at the same hook points.
7. Played by bots with a golden digest; labs as `se-q2-fight` and `se-q2-coop`.

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| Co-op with companions who follow, defend and press buttons | ours, over both campaigns | `shooterbots/companion.hpp` |
| Saves of the whole running game, bit for bit | ours | `engine/save` |
| Quake II's monsters in Quake's campaign and back | ours | `rules/campaign_q2.hpp`, header-only rosters |
| C++ mods stacking on any game's rules | ours | `engine/mod`, the seam |
| Mod extensions checked by name | Quake's later engines | `plugins/progs/extensions.hpp` |

| Drawback left behind | How |
| --- | --- |
| Game logic in a scripting language | our rules in C++; the VMs only run the community's bytecode |
| A native game library that can do anything on the machine | hosted for compatibility; our own mods sandboxed in the store form |
| A server that is one program form only | a runner table: any form on any host |

## Gaps

A game library's single player (a mission pack's) keeps its levels in the unit and saves by the
library's own files packed into the engine's save; a frame cut to what a client sees is open.
A rotating brush model is one turned brush in every game (`plugins/shooter/turned.hpp`): Quake II's
server and its client's prediction trace it in its own frame, the frame every game draws it
turned in (Scourge of Armagon's spinners, Quake II's fans and hinged doors). Quake II's single-player playthrough stuck at breakable glass; the third game's module VM
server for its mods' client half and menus. Quake's single player against the original: the
ranked list in `porting/shooters-edge/q1-sp-gaps.md`, every row landed but the two kept by
choice (the fixed 1/72 s step and co-op switched on during a map). Co-op on every campaign map
(joins, a death, a leave and rejoin, the keys, the exit, the ending) is held by `se-coop-sweep`.
The liquids (lava and slime's ticks and clock, the sounds going in and out, the burns' cries, the
gasp, drowning that grows, the liquid slowing a swimmer) are held to Quake's own program by
`se-dive`; the ticks are the harm rows of `plugins/shooter/liquid_harm.hpp`. Quake II's second gap
list (`porting/shooters-edge/q2-gaps2-rules.md`) has its rank-1 rows landed: the obituaries, the
death, join, leave and limit lines word for word, the next map, the spawn discs, co-op's spots and
its carried state over the network.

Quake and QuakeWorld as a whole: `porting/shooters-edge/q1-qw-ledger.md` lists every command,
setting, builtin, message, menu page, entity, item, weapon and deathmatch rule of the stock
releases, marked done, missing or not taken, and is THE gap list (its missing count only goes
down). Quake's deathmatch without a mod of its own plays the arena's rules in C++ with Quake's
own on top (`rules/common_quake.cpp`), switched on by the local info `rules q1` that the game's
profile writes (`GameProfile::arena_rules`): the body's pain, firing and death frames, the cries,
gibs and the four corpses kept, the spawn drawn at random with its flash, the kill command, the
damage order and the teamplay modes, each mode's respawn times and weapon stay, the weapons'
numbers and sounds, the liquids' sounds and drowning, the keys and key doors, the secrets,
relays and counters, the exits, the intermission and the next map, and the kit carried to it.
The third fill adds the rest of the rules a deathmatch player meets: a door's crush credited to
whoever opened it, the pentagram against a telefrag ("Satan's power"), the quad and ring dropped
by the info keys `dq` and `dr` with everyone told who lost and who recovered them, the switch to
the best weapon after ammunition, the OctaPower's own warning, a spectator's impulse 1 through the
spots, named lights that switch and the fluorescent ones that hum, triggers with an angle or with
health, and a body that slows on the ground (`tests/shooters_edge_arena_quake_test.cpp`).
The arena's own rules, its digests and its pins are unchanged without the switch; `se-quake-dm`
plays both from one seed.

Quake II's mission packs play their rules in C++ beside the base game's, behind the pack switch
(`server/q2_pack.hpp`: the game folder picks it). A pack's items, weapons, shots, monsters and
map things are rows that carry their pack, and only that pack registers their models and sounds,
so the base game's lists, goldens and digests do not move. What two games share is one table:
the timed powers (`server/q2_powers.hpp`: the quad, The Reckoning's dual fire, invulnerability,
the suit and the rebreather, each with its tint, warning, shell and drop rule) feed the timer, the
view's tint, the shell, the drops and `use` alike. The Reckoning's rules are in
`server/q2_reckoning.hpp` (its map things, its thinking shots, the trap) and `server/q2_choose.cpp`
(its weapon cycle and the hyperblaster and railgun toggles); the item and ammunition tables are
saved length first, so a pack's rows never break an older save.

## Labs and tests

`se-baseline`, `se-quake-dm`, `se-campaign`, `se-coop`, `se-coop-episode`, `se-coop-sweep`, `se-dive`, `se-q2-fight`, `se-q2-coop`,
`se-q2-save`, `se-q2-unit`, `se-mod`, `se-qvm`, `qc`, `mod`; `tests/shooters_edge_rules_runner_test.cpp`,
`tests/shooters_edge_quake_rules_test.cpp`.

## Lessons

- A second rules runner reads the map's own entities, mirrors its world into the loop's entities,
  and counts the host's time by the call. Every field the loop reads must be in the mirror, or it is
  silently zero.
- The window asks the rules' world or a runner's trait, never which runner it is.
- A monster swapped whole is the other game's numbers as data in the played rules' own shape.
- A game's own rules on shared rules go behind one switch the game's profile writes, read once a
  frame; every branch tests it, so the shared rules' digests stay put without it.
