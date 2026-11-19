---
type: Fixed
title: Quake II's windows are see-through
---

In Shooter's Edge, a face a version 38 map marks as one or two thirds opaque, such as a window or a display's screen, is now drawn see-through at that opacity and as painted, in both looks, where it was drawn solid and lit by the wall's light. The brush plugin's draw kinds gain `kGlass` and `kGlassThin`, and the culling compute pass counts ten kinds.
