---
title: Grade a cut a couple of cells past its ends, continuing its pitch, or the grid misreads it
date: 2026-10-21
scope: plugins/mountain/mapgen/speed_ski.cpp
concepts: [speed-ski, heightfield, grading, bilinear, sweep]
---

The speed track graded the ground to its profile only between its two ends, and held the profile
flat past them. A point at the track's very first station is read bilinearly from grid nodes up to
a cell (more, on a track at 20° to the grid) behind it, so on a steep top the ground there read
0.3–0.4 m under the profile, on 6 of 30 seeds. Grade full weight two cells past each end, and past
the profile's own samples continue it at its end's pitch. Whatever is cut into a heightfield and
then checked against it must be cut wider than the check reads.
