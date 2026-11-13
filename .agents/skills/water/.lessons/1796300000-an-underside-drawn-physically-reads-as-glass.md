---
title: A water underside drawn physically reads as glass; draw the two surfaces a player knows
date: 2026-11-12
scope: engine/render/water_underside.wgsl
concepts: [water, underside, open-below, snell-window, total-internal-reflection, look, kshowwaves, ksheen]
---

The exact optics of a still pool hung over air (Fresnel off the bottom a few per cent, every ray
through a flat slab leaving the top) drew a dim brown pane the player did not read as water. What
reads: the bottom drawn as water seen from over it (slopes shown steeper for the look only, a sheen
floor on the mirror of the room under it, lamp glints) and the top drawn as water seen from under it,
the eye's ray carried in at its OWN angle so Snell's window and total internal reflection appear.
Name such choices as a look in the shader's header (`kShowWaves`, `kSheen`, `kBend`) and keep the
geometry and anything a float reads on the true waves. Also: the view up's projection only covers
exits near straight over the eye; a clamped read smears its edge into streaks, so fall back to the
screen (march) where the projected point leaves the view.
