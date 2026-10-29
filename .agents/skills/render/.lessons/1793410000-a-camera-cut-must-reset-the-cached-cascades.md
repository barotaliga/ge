---
title: A camera cut must reset the cached cascades, or the near shadows stay where the camera was
date: 2026-10-29
scope: games/fall_line/app/frame.cpp, engine/render/shadows.hpp
concepts: [shadows, cascades, reset-sun-shadows, camera-cut, still, moving, casters, caster-slots, village, lab-views]
---

The sun's cascades are redrawn only when the eye drifts past a share of their radius, so a lab
that films views far apart, or a cut in the game, keeps the near cascade round the last camera:
the cars 25 m in front of the new one fell into the far cascade and cast nothing visible. A cut
promises the next frame keeps nothing of the last, so `cut` resets the cascades too. Choose a
caster's kind by how it moves: what stands goes in `still` (the cascades, `kCascades` slots),
what moves in `moving` (the atlas, `kMaxShadowFigures` slots, culled per tile), and a flat sheet
on the ground in neither. A family's caster slots are made before `bind_family_hiz`. A lab that
shows a moving caster's shadow must stand a figure beside it, or no tile is there to draw it.
