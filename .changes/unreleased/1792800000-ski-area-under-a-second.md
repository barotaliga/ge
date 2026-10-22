---
type: Changed
title: A ski area builds in under a second
---

A ski area (v10) now builds in about 0.93 s a seed in the 4-core container (was 1.55 s), and a single piste in about 0.22 s (was 0.26 s), with every map's bits unchanged. The ski routes' search no longer reprocesses stale heap entries and works each step's felled metres once, the network index skips segments and cells that are too far away to matter, and the pure readings (the routes' two searches and their candidate lines, a drag lift's candidate places, the woods' scan, a run's stamping) run on threads with their answers taken in their old order.
