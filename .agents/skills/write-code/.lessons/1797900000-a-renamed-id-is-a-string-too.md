---
title: A renamed id lives in string literals, line breaks and GPU labs too
date: 2026-11-20
scope: games/shooters_edge, docs
concepts: [rename, profile-id, string-literal, prose-sweep, gpu-lab, digests]
---

Renaming a game's id (`"classic"` to `"q1"`) is not done when the identifiers compile. The id
also lives in string literals a lookup takes (`find_game("classic")` in a GPU lab crashed on a
null profile; precheck reached it only on a later change), in paths (`data/classic/`), in the
cvars' values (`sv_wear`, `sv_physics`, the `cr_` switches) and in carved-word keys
(`game-classic`). Grep every `"old"` literal and check each one by hand: some are the family's
word and stay (the cvar layer, the look). A prose sweep run line by line misses a phrase broken
across a comment's line end ("the classic\n// game's"): sweep the whole file with a multi-line
pattern (perl -0) as well.
