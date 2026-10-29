---
title: A cloud shot says where the eye stood against each layer, or it cannot be read
date: 2026-10-29
scope: games/fall_line/labs/gpu_world_lab.cpp, plugins/mountain/app/cloud_field.cpp
concepts: [clouds, cloud-height, thickness, inside, eye, stamp, density, thin-patches]
---

A shot taken inside a deck and one taken in a gap under it look alike: clear air round the
skier, cloud far off. The `world` lab now stamps the eye's height and each layer's base and
top over the floor, which settles it. Inside a sheet the near field is as clear as its thin
patches make it (`CloudLayer::thin`, the map's `thick_bias`): a stratus at the map's own lean
leaves a few hundred metres clear, and only from `sv_clouddensity` 0.5 up does it read as
being in cloud. Judge the in-cloud look at a picked density, not at a map's lean.
