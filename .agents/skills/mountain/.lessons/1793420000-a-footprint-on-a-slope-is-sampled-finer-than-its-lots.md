---
title: A building's fall is read every metre under its walls, not at the lot's coarse grid
date: 2026-10-29
scope: plugins/mountain/village/buildings.cpp
concepts: [buildings, footprint, fall, plinth, sample, bilinear, test, tolerance]
---

The placer tests a footprint's fit (off the streets, clear of the runs and lifts) every
`buildings::kSample` m, which is fine for a mask, but the ground's highest and lowest point under
the walls read at that grid missed a rise between samples: a test sampling at width/6 found ground
above the floor `y` and the floor sank into the snow. The floor and the plinth's foot are now read
every metre within the walls, and the test allows 0.05 m for the heightfield's bilinear sag between
those metres. A coarse grid decides WHERE a thing may stand; what it stands ON is read at the
ground's own resolution.
