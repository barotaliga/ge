---
title: The replacement table says what the engine draws instead, and the mod has to spawn the thing first
date: 2026-11-05
scope: plugins/classicenhance/replace.hpp
concepts: [replacement-table, flare, bubbles, light-globe, air-bubbles, sprite, campaign, arena]
---

`replace.hpp`'s table had no reader: Shooter's Edge hard-coded the torches by file name and the
explosion by its own switch. Reading `replacement_of(name)` into each model kind at load
(`ModelKind::effect`) makes the table decide, and `fx_switches` checks the table's row for the
explosion sprite. Two traps before a replaced effect can be seen at all. The tree's mod spawned no
`light_globe` (its `common/map.qc` now stands the sprite statically, like a torch). And
`air_bubbles` lives in the campaign variant and removes itself in deathmatch, so `se-models` (the
arena by default) needs `--progs=games/shooters_edge/qc/campaign/progs.src`. On the pack: e1m4
has two bubble spawners, e4m7 two globes (scan the pack's entity lumps for the classname). A
globe hangs on its wall: aim at its middle, not 16 units over a floor.
