---
name: shooterbots
description: "Use for plugins/shooterbots in gameengine2: the bots any shooter on the kit links. The pilot and the nav proven by the move, the senses and the ears, the aim, the dodge, the escape and the brink, temperament and traits, features, team talk's meanings, the command-source seam a server slot reads, and the movement recipes read off people (hops, gap and rocket jumps, hop starts, the view book). Load the bots skill beside it for how a bot is judged and debugged."
---

# The shooter's bots

`plugins/shooterbots/` (`ge_shooterbots`, namespace `ge::shooterbots`) holds the bots that play
through the shooter kit: `ge_add_plugin(shooterbots ENGINE core ai PLUGINS shooter brush)`. It
sits above `plugins/shooter` (the move, the hulls, the server's world; `brush` for a map's
entities and brushes) and on `engine/ai` (the
decision record, the hands, sight, memory, paths, choosing). The kit never includes it, so a
shooter of our own can link the kit without bots. Everything a bot decides that is no one game's
is here too: the fighter, its mind, what it knows, the map's plan and nav. A game hands its bots
its `BotArsenal` (its weapons and item rows, the words and sounds they hear, its maps' book of
people's tricks and routes, its team words) and keeps only that data and its runs over its own
server (Shooter's Edge's `games/shooters_edge/bots/`; `docs/porting-games.md` § Bots: what is
shared and what is each game's).

**How a bot is judged, debugged and made more human is the `bots` skill.** This skill is the
plugin's layout and rules.

## What is where

`make find Q="<words>"` finds a file by its tags; the groups:

- **Moving:** `nav` (floor nodes and links proven by `player_move`), `nav_field`, `nav_gaps`,
  `nav_script`, `swim`, `jump_finder`; the pilot (`pilot.hpp`, its parts `pilot_*`, the
  movers, the gaps, the recipes), `look_ahead`, `brink`, `escape`, `circle_jump`, `idle`.
- **Fighting:** `bot` (the pilot with senses and aim, a slot's command source), `aim`,
  `intercept`, `dodge`, `senses`, `hearing`, `temper`, `traits`, `features`.
- **What a bot reads:** `bot_view` (a match by reference), `cmd_source` (the seam a server slot
  fills), `team_talk` (what a team message means; each game words it).
- **Read off people:** `recipes`, `hops`, `gap_jumps`, `rocket_jumps`, `view_book`,
  `hop_start_book` (people's hop starts as the pilot's recipe book).
- **Deciding (the fighter):** `fighter` (mode, goal, weapon, trigger) and its parts
  (`fighter_parts.hpp`: `fighter_items`, `fighter_doors`, `fighter_goto`, `fighter_setup`),
  `mind` (team lines, roles, calls), `team`, `knowledge` (the items seen, recalled, timed),
  `ears`, `packs`, `foe_ledger` (its own might and each enemy's), `watch`, `spam`,
  `launcher_fire`, `keyboard`, `footwork`, `water`, `buttons`, `floor_trap`, `handles`.
- **The map:** `map_nav` (the nav built off a map's hulls or brushes, movers, teleporters,
  water), `map_plan` (areas, worth, ways in), `goals`, `way_through` (a campaign map's exit),
  `entities` (the map's entity text); `companion` (the co-op companion).
- **What a game hands its bots:** `bot_arsenal` (`BotArsenal`), `item_tables`, `map_book`
  (people's tricks, routes and item rules by map), `team_talk`'s `TeamWords`.

## Rules

- **The kit's names, unqualified.** `kit.hpp` puts `ge::shooter` in view of `ge::shooterbots`
  with one using-directive and forward-declares the kit's types a header names. Every file here
  includes it; nothing else does. Outside the plugin, name a bot's type `shooterbots::X` (or
  `sb::X` beside the kit's `sh::`), never `shooter::X`.
- **Never the other way.** Nothing in `plugins/shooter` includes this plugin;
  `tools/check-plugins.mjs` fails the cycle.
- **One move per game.** A trick tuned to one game's numbers is that game's file, not a knob
  here (`docs/porting-games.md` § Do not over-generalize).
- **A game's data reaches the bots through its arsenal, never a call into the game.** A plugin
  links alone, so nothing here may name a game's function (`first_arsenal` is the game's): the
  caller passes the arsenal (`fighter_init`) and the maps' book (`build_map_nav`, `map_goals`).
- Tests: `tests/shooterbots_<file>_test.cpp`; goldens `tests/golden/shooterbots_*.txt`.
