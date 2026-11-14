---
title: A platform a bot cannot climb is a wall: keep ramps under 30 degrees and steps under 18
date: 2026-11-14
scope: games/shooters_edge/content/maps/keeps.plan, plugins/brush/compile/plan.cpp
concepts: [map-plan, ramp, platform, bots, se-team, quad, item-reach]
---

The Keeps' quad stood on a platform 40 units high with ramps 48 long (a 40 degree slope): a
player could walk it, `se-team` reported "items no route leads to: none", and still no bot took
the quad in ten minutes; the trails from above circled the platform and never crossed it. A
platform 16 high with ramps 64 long fixed it. Read the trails picture (`map.png`) and the
`team.<side>.quads` / `took-per-min` metrics after every map change, not only the route line.
