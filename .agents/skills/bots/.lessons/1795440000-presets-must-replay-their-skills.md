---
title: Turn a skill into traits by presets that replay it to the bit, and split every skill read by its subject
date: 2026-11-02
scope: plugins/shooter/traits.cpp, plugins/shooter/aim.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/mind.cpp
concepts: [traits, presets, tier-of, scale, goldens]
---

Replacing one skill knob with many traits is safe when every preset reproduces the old skill
exactly: put the presets on the old table's rows (2, 5, 8) and read a smooth trait through a
`scale` that returns the row itself at those points. Then the goldens are the proof: every one
stayed unchanged, so any later move is the traits' own doing.

One `MapSense` gated thirty unrelated choices (what is said, what is timed, who holds an area,
what a sound means). Classify each read by its subject before splitting it; a name already taken
on the fighter (`team`, the team game's flag) silently compiles into the wrong comparison, so
pick a fresh one (`teamwork`).

A lab table feeds the lab's digest: a new table moves the digest without any behaviour moving.
