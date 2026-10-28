---
type: Added
title: Classic vertex-animated models and sprites read
---
A new plugin, `plugins/morph` (`ge::morph`), reads the classic vertex-animated model files and sprite files into one view: skins and their groups, vertices split on the skin's seam, counter-clockwise triangles, and every frame as a full set of positions with its group and interval; sprites' frames and facing. Every count and size is checked before anything is read, and damage is refused with what was wrong. `frame_normals` gives each frame smooth vertex normals shared across the seam; `smooth_model` optionally rounds the facets into point-normal patches. Fixture writers make test files, and the `morph` lab draws a low-poly body flat, smooth-shaded and rounded. `write_glb` writes a model as one binary glTF with a morph target a frame and its clips as animations of the morph weights.

The classic-enhance pipeline gains a `models` stage: every model in the pack written to the cache as glTF with its skins enhanced and smooth shading (`--round=<n>` rounds the facets, off by default), every sprite frame as a PNG, and `replaced.txt`, the effects the engine draws in place of the pack's explosion, flare and bubble sprites and its torches' painted flames.
