---
type: Added
title: Classic vertex-animated models and sprites read
---
A new plugin, `plugins/morph` (`ge::morph`), reads the classic vertex-animated model files and sprite files into one view: skins and their groups, vertices split on the skin's seam, counter-clockwise triangles, and every frame as a full set of positions with its group and interval; sprites' frames and facing. Every count and size is checked before anything is read, and damage is refused with what was wrong. `frame_normals` gives each frame smooth vertex normals shared across the seam; `smooth_model` optionally rounds the facets into point-normal patches. Fixture writers make test files, and the `morph` lab draws a low-poly body flat, smooth-shaded and rounded.
