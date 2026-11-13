---
title: A classic game's server words are its profile's table, said through `words`
date: 2026-11-05
scope: games/shooters_edge/profiles/words.hpp, games/shooters_edge/server/words.hpp, games/shooters_edge/server/announce.hpp, games/shooters_edge/rules/arena_glue.cpp
concepts: [classic-words, obituary, countdown, words-builtin, localinfo, redtext, gold-digits, lobby]
---

Every line a loaded classic game's server prints is a template in `profiles/words.hpp`, travelling
as the server's local info (plain ASCII: `^red^`, `%g1` gold digits, `%[ %]`, `%>4g1`, `%n`;
`words_fault` refuses a quote, a backslash or a high byte). The baseline mod says one with
`game_words(key, ...)` (the `words` builtin, #640) and falls back to its own words on "", so
Shooter's Edge's content is unchanged; a new message is a key in the table and a `game_words`
call in the arena's glue, never English in the common QC. The lobby's lines become events through
`server/announce.hpp`, which reads the engine lobby's `notes` (what each `said` line is about),
not the line's text. A variant draws from the game's stream only when the game names two or more,
so our own runs keep their bits. The clean-room spec they come from is
`porting/shooters-edge/messages.md`. `plain_text` must fold the gold figures (18..27, 146..155)
to digits, or a gold number vanishes from a plain line.
