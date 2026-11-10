---
title: Smoke lit without the smoke between it and its light draws as a flat disc
date: 2026-11-10
scope: engine/render/explosions.wgsl
concepts: [explosion, smoke, volumetric, self-shadow, chord, beer, ray-march, indoor, key-light]
---

A ray-marched ball with a noisy edge still read as a flat grey disc in a room: its smoke took the
same light at every point (a dot product with the sun stood in for shade, and indoors there is no
sun). What reads as depth is the shade a cloud casts on itself. Per step: the ball's analytic
chord toward the key light (the big terminator, free) plus two billow reads at 0.25 and 0.6 radii
(the lobes on each other), the chord up for the sky, one read half way to the core for the fire's
glow; Beer mixed with exp(-0.25 tau) so the shade never goes black. Indoors the key light is
straight down. Skip the reads where the step is pure fire. It priced 1.3-2.5x the pass on the
software adapter, still under 5% of the frame. Judge it zoomed (crop and upscale a lab's frame):
at the lab's 640 x 360 a blast is a few dozen pixels and the change hides.
