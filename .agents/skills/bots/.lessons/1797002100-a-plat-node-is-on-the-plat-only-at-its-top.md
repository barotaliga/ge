---
title: A node on a plat was reached at any height, so bots walked plat links from under the raised slab; on the plat means the feet at its top now
date: 2026-11-10
scope: plugins/shooter/pilot_movers.cpp, plugins/shooter/pilot.cpp
concepts: [plat, mover, ride, reached, plan-from, nav-nearest, silent, under, raised, legs, loop, client-fed, lag, dm2, trial, a-b]
---

- A plat's nodes stand where its top is at REST. A bot on the floor under the raised plat
  (dm2's two plats are slabs on legs) is near them and on none, yet `reached` counted a mover
  node at any height and plans started from the nearest node, so links across the top were
  walked into a leg, stepped clear of, and walked again every 0.6 s. The maintainer's test is
  right: a player walking onto a plat RISES a step, so on it means the feet at its top now.
- A moving plat's box as the client has it lags its rider by up to a frame's travel (~15 u). A
  strict "feet within 8 of the top" took riders for under it and replanned them off mid-ride
  (a set's speed -13 u/s); the tolerance is wide while it moves (`kRideLag`), strict when
  silent.
- A fallback start node must be one the bot can walk to: the nearest off the plat by the
  columns was across a wall, and the bot replanned every other command for 8 minutes. Require a
  clear straight hull trace within `lost`.
- Read se-team's "where they stand still" table on every A/B set: a whole match collapsing
  (both teams' frags low) was one bot stuck in one 64-unit cell, not noise.
- A/B a pilot fix by gating it on a temporary `PilotSettings::trial` read off the `trial`
  feature, every part of the change behind it, then make it unconditional.
