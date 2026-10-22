---
title: Answer geometry finer than the grid off its own profile, and cut the grid beneath it
date: 2026-10-22
scope: plugins/mountain/mapgen/ski_jump.cpp
concepts: [ski-jump, venue, analytic-surface, heightfield, ground-at]
---

A ski jump's table edge has a 3 m wall under it and its landing hill a 300 m radius; baked onto
the 2 m grid, bilinear reads round the edge off and turn the radius into facets. So the hill
answers `ground_at` and `normal_at` off its own profile (`Level::venue`) inside its width, and
the grid under it is CUT to the surface's lowest minus a margin: anything that reads the raw
grid (a picture, a mesh, a lab) then sees ground at or below the hill, never through it. Hold
both in the test: the grid never stands over the surface along the line, and the ground beside a
raised deck lies far under it.
