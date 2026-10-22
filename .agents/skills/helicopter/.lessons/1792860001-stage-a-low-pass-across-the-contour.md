---
title: Stage a low pass across the contour, never down the fall line
date: 2026-10-22
scope: plugins/helicopter/scenarios.cpp, plugins/helicopter/pilot.cpp
concepts: [scenario, tree, terrain, sink-rate, fall-line, low-pass]
---

The `tree` scenario first flew the bot down the slope at 8 m to meet the lone tree's crown. It
never did: the pilot's sink is capped (`pilot::kSink`, 6 m/s), and at cruise speed down a 17
degree face the snow falls away faster than that, so the machine passed some 30 m over the
crown. A low pass that has to stay low is flown across the contour, where the ground under it
stays level; the scenario now crosses the face at the tree's height.

The same reading for the `terrain` scenario: its `lowest` counted the climb off the snow at the
start. A scenario about holding height starts in the air at that height.
