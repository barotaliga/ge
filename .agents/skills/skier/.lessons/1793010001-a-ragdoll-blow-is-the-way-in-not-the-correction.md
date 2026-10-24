---
title: A thrown body's blow is the way a point came into the step, never its solver's corrections
date: 2026-10-23
scope: plugins/skier/ragdoll.cpp, plugins/skier/body.cpp
concepts: [ragdoll, thrown, struck, impact, trunk, neck, helmet, blow, verlet, constraints]
---

The first cut measured a point's closing speed on a trunk off its move through the solver's
passes (where it is less where it was), and judged it every step it touched the trunk. The
joints' and the bones' corrections then counted as speed, a body lying against a trunk was
struck again every step, and the slow-trunk scenario (6 m/s) died of a broken neck. Measure
the blow as the snow's already is: the point's own way into the step (kept before the passes),
and only on a fresh contact (`Ragdoll::leaning`, as `planted` for the snow). A point must still
be pushed out every step; gating the push by the fresh contact let the body sink into the trunk.

The neck's share of a head blow is the head's stop in its helmet, not the neck's own give: the
neck is loaded by the head it carries. Judged on the neck's give alone, every head-first meeting
broke it.

Read a body change off the `injuries` lab's sweep over draws, and probe a curve over speeds
before trusting one scenario: one run is one draw.
