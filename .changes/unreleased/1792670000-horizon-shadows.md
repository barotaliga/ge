---
type: Added
title: The mountain's own shadow
---

The terrain now shades itself: `engine/render/horizon.hpp` keeps two horizon maps either side of the sun's bearing, baked by compute and rebaked one at a time as the sun turns, and the terrain's draw blends them into soft ridge shadows that move with the hour. The `shadows` lab draws the maps from above and counts the bakes of a sun swept across a day.
