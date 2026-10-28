---
type: Added
title: Trampled grass that stands back up, and a field that shades itself
---

`engine/grass/trample.hpp` lets a game press grass down where a foot, a wheel, a body or a landing meets it (`stamp`): the blades there lie along the way they were pressed, splayed at the edges, and stand back up over their kind's new `spring` (a lawn in 3 s, long grass in 25 s). The press is a function of the cells and the clock, so a replay draws the same trail. `render::set_grass_trample` hands the same cells to the GPU, where a pressed blade is lit to its root and shows its paler back, so a trodden path reads lighter than the field. Grass now shades itself: the sun reaching a blade is dimmed by the blades above it, so a high sun lights long grass halfway down and a low one only its tips, with the rest glowing through from behind. The fine blades now write their own motion vectors, so temporal anti-aliasing keeps a waving field sharp. The `gpu-grass` lab's `trampled` view shows a path and two wheel tracks lying down and standing back up.
