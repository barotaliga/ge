---
type: Added
title: Shooter's Edge's shadows, water and skies at low, medium or high, and r_quality over them
---

The three dearest classic-enhance switches each have a level beside them: `ce_shadows_quality`, `ce_water_quality` and `ce_skies_quality` take `low`, `medium` or `high` (the default, the whole look), and each stop is cheaper than the one over it. `r_quality` picks all three off Shooter's Edge's quality ladder (`low`, `medium`, `high`, `top`, `auto` inside `r_budget` ms, or a number from 0 to 1); `custom`, the default, leaves the switches and levels as set. A stop changes on the next frame with no pipeline made. The `se-timedemo` lab's `--price-stops=1` prices every stop. The engine's water takes a `WaterQuality` (the chop, the sun's caustics and shafts, the reflection), and the brush renderer's options say how many lights the moving casters shadow, whether their edges are soft, and whether the capsules are drawn.
