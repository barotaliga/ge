---
title: A game's own deathmatch rules ride on the arena behind one switch its profile writes
date: 2026-11-21
scope: games/shooters_edge/rules/common_quake.cpp, games/shooters_edge/profiles/game_profile.hpp, tests/shooters_edge_quake_rules_test.cpp
concepts: [quake-rules, rules-q1, arena-rules, localinfo, variant, digest, ledger]
---

Quake's deathmatch rules are not a third program: they are branches in the arena's own rules,
each testing `quake_rules(q)`, which `quake_read` sets once a frame from the local info `rules
q1`. The game's profile names the rules (`GameProfile::arena-rules`) and the window and the
dedicated server write the line beside the game's words, so the arena's pins and digests do not
move without it and a test turns it on with one console line. The rule that must hold is that the
switch is read in `worldspawn` BEFORE the map's entities spawn (item respawn times, key doors'
`wait`, the triggers' sounds are decided at spawn). Things only the arena variant needs (a level
exit's `map`, an intermission spot's `mangle`, a counter's `count`) go in `SE_ARENA_FIELDS`, not
the common list, so the campaign's layout stays where it was. Work the gaps from the ledger
(`porting/shooters-edge/q1-qw-ledger.md`): flip each row a PR fills, and recount.
