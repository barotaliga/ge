---
type: Added
title: gpustats lists every pass and the GPU time by tenth of the run
---

`gpustats 600 all` reports every timed pass instead of the dearest dozen, and every report over 20 frames or more adds the GPU frame span's median in each tenth of the run, so a GPU that slowed partway (a power state, a clock drop) shows as a step.
