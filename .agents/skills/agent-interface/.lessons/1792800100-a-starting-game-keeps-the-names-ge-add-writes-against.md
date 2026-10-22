---
title: A starting game keeps the names the ge add templates use
date: 2026-10-22
scope: templates/game-racer
concepts: [scaffold, ge-new, templates, ge-add, racer]
---

`ge add lab` and `ge add test` write code against the scaffold's simulation: `GameState` with
`x_m` and `vx_mps`, `create_game(seed)`, `step(state, Input{steer})`. A starting game under
`templates/game-<name>/` must keep those names compiling (the racer keeps `x_m`, `vx_mps` and an
`Input` whose first field is the steer, with a throttle that defaults to flat out), or a repo it
starts breaks on its first `ge add`. Prove it the way packaging's `scaffold` job does: `ge new
--game=<name>`, every `ge add` kind, then `ge check`.
