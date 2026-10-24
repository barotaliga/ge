---
title: Measure how much of a subject a picture shows off the depth, not the colours
date: 2026-10-23
scope: engine/render/instances.wgsl, games/fall_line/labs/gpu_camera_world_lab.cpp
concepts: [see-through, dither, depth, visibility, occluder-fade, camera-world, lab-metric]
---

A dithered fade looks right or wrong by eye, but a number needs the depth target: project
points down the subject's middle with the frame's own `view_proj` (camera-relative, column-
major, reversed-z) and count each seen when the depth there is no nearer than the point moved
half a metre toward the eye. The skier's own figure then counts as seen and a crown before him
does not. With the crowns seen through, seed 38's densest wood went from 0-33 % of him seen to
80-93 %, and the piste's sheets stayed at 100 %, which is the before-and-after the PR shows.
Keep the fade a share of the depth prepass under the LOD band's own dither: the colour pass
tests `equal` and so follows it with no change, and the shadow casters, which have no fragment
stage, keep the whole tree.
