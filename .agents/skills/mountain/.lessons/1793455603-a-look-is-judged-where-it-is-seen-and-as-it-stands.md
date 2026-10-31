---
title: A material is judged where the sun is on what the camera looks at, and a unit mesh's faces as the instance stands them
date: 2026-10-22
scope: games/fall_line/app/views.cpp, plugins/mountain/app/trees.cpp
concepts: [snow, material, close-view, sunlit, judging, trees, unit-mesh, instance-scale, aspect, snow-by-facing]
---

- A close view of the snow stood on a sunlit spot and still came back dark blue: the snow it LOOKED
  AT lay in a wood's shadow or faced away. In shade the corduroy, glare and glitter vanish and the
  review learns nothing. A material view checks the points along its heading (facing the sun, past
  the mountain, clear of the trees' shadows), takes the best spot when none passes, and runs at a
  few hours.
- The tree meshes are unit-sized and an instance stretches them about 4.5 times as tall as wide, so
  classifying snow by the unit normal painted almost every tier white and a round limb came out a
  ribbon. Judge a face by its standing normal (y divided by the aspect, renormalised) and build a
  round cross-section with its y offsets divided by the aspect.
