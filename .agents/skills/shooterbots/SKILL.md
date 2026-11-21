---
name: shooterbots
description: "Use for plugins/shooterbots in gameengine2: the bots any shooter on the kit links. The pilot and the nav proven by the move, the senses and the ears, the aim, the dodge, the escape and the brink, temperament and traits, features, team talk's meanings, the command-source seam a server slot reads, and the movement recipes read off people (hops, gap and rocket jumps, hop starts, the view book). Load the bots skill beside it for how a bot is judged and debugged."
---

# The shooter's bots

`plugins/shooterbots/` (`ge_shooterbots`, namespace `ge::shooterbots`) holds the bots that play
through the shooter kit: `ge_add_plugin(shooterbots ENGINE core ai PLUGINS shooter)`. It sits
above `plugins/shooter` (the move, the hulls, the server's world) and on `engine/ai` (the
decision record, the hands, sight, memory, paths, choosing). The kit never includes it, so a
shooter of our own can link the kit without bots. A game's own bot (Shooter's Edge's fighters,
its item worths, map plans and each game's footwork) stays in the game
(`docs/porting-games.md` § Bots: what is shared and what is each game's).

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
- **Read off people:** `recipes`, `hops`, `gap_jumps`, `rocket_jumps`, `view_book`.

## Rules

- **The kit's names, unqualified.** `kit.hpp` puts `ge::shooter` in view of `ge::shooterbots`
  with one using-directive and forward-declares the kit's types a header names. Every file here
  includes it; nothing else does. Outside the plugin, name a bot's type `shooterbots::X` (or
  `sb::X` beside the kit's `sh::`), never `shooter::X`.
- **Never the other way.** Nothing in `plugins/shooter` includes this plugin;
  `tools/check-plugins.mjs` fails the cycle.
- **One move per game.** A trick tuned to one game's numbers is that game's file, not a knob
  here (`docs/porting-games.md` § Do not over-generalize).
- Tests: `tests/shooterbots_<file>_test.cpp`; goldens `tests/golden/shooterbots_*.txt`.
