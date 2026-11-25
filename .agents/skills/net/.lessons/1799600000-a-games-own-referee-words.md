---
title: Give the shared referee a game's words, and leave out the words a mod may own
date: 2026-11-23
scope: engine/server/referee.cpp
concepts: [referee, admin, per-game-words, qw-referee, mod-words, line-cap]
---

The engine's referee answers `true` for every word in its list, even to a player who is not a
referee ("Only a referee can do that"), so a word in the list is taken from the mod for good. A
classic game's table (`RefereeWords::words`) names only its own words: QuakeWorld's has
`forcebreak`, `forcemap`, `kick` and `acommands`, and `tl`, `ban` or `mute` still reach a mod.
Run the book only on the server's own program (`progs_crc`, `module` and `native_library`
empty): a loaded match mod has its own `admin` and `kick`. Without sv_lobby the QuakeWorld server
is always "in a match" for `forcebreak`, but a map change is never refused for it.
`qw_game.cpp` sits at the 2000-line cap: new QuakeWorld server features go in a file of their
own fed by a small facts struct the game fills (`QwRefereeGame`), not in `qw_game.cpp`.
