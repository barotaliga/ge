---
type: Added
title: Shooter's Edge draws see-through entities
---

An entity a mod or a map makes see-through (`alpha` under 1) is now drawn see-through: in the window's own game (a QC mod's `alpha` field, answered as `DP_ENT_ALPHA` by `checkextension`, or a map entity's `alpha` key), on a joined server and in a played demo (the transparency byte, read rather than dropped). The model pass leaves out a share of the body's pixels by an ordered pattern that moves every frame, and TAA-U averages it into a see-through body, so nothing needs sorting. A mostly see-through body casts no shadow.
