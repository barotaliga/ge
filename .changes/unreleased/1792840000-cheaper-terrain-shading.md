---
type: Changed
title: Cheaper terrain shading, the same picture
---

The terrain's colour pass costs about 5% less with no visible change: the figures' shadow tiles are found by an offset per tile from one transform of the point (and skipped wholesale where no tile reaches), and the ski tracks far enough away that a groove is under a pixel are read with one tap instead of five. `ShadowParams` gains `reach`, and `tile` now holds each figure's offset in the atlas's clip units.
