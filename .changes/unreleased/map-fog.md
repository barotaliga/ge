---
type: Added
title: Shooter's Edge draws a map's own fog
---

A map whose settings carry a `fog` (density, then red, green and blue, then an optional alpha and bias) is drawn in that fog: the far walls sink into its colour, models, particles and water with them, and the sky takes a share of it (the map's `skyfog`, else `r_skyfog`, 0.5). The `fog` console command takes the same words and holds until the next map. The engine gains the flat fog behind it (`FrameView::flat_fog`), and shaders larger than one string literal's 60 KB now embed whole.
