---
type: Changed
title: Shooter's Edge's fires, lava and windows glow subtly, and a bonfire's sparks swirl
---

The modern look's bloom is 0.08 (was 0.3), the lava lights its walls at 0.02 (was 0.2), a classic map's lava burns at 1000 C, the glowing texels (windows, lamps' panels) at 0.6 of the classic's fullbright (`r_glow`), the flames at 8 times a lit wall's light (`r_fire_glow` scales them), and every map light is softened near it over 0.4 of its half reach, so a torch's wall shows its flame and not a white disc. A bonfire's sparks are tiny glowing beads at a wood fire's heat, about a dozen a second: flecks thrown out and whirled about, floaters of ash wandering up on the plume, and an ember or two popped out at once that arcs back down onto the floor, each riding the air's eddies (`Particle::swirl`). The `se-client` lab's `--film=<frames>` keeps each view running as an animated GIF.
