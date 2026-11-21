---
title: Check a classic look against the game's own light rule pixel by pixel, not by mean luma
date: 2026-11-21
scope: plugins/brush/app/bsp.wgsl
concepts: [classic, brightness, tone-curve, toe, display, palette-grade, q2, formula, murky, post]
---

"Quake II looks dark and murky" survived a mean-luma check (classic and modern agreed at about
32) because both looks were wrong the same way. The `se-q2` lab's `<map>-formula` shot draws the
game's rule on the CPU (`brush/labs/textured_view.hpp` with `palette_glows` false, `coloured_light`
true, `gamma` 1; the world's own surfaces only, since the GPU's world pass leaves the doors to the
entities) between the two looks, and the median per-pixel ratio found three faults a mean hid:
the Khronos neutral tone curve's TOE (`x - 6.25 x^2` under 0.08) takes the least channel of a dark
colour to black, so painted-dark greys and blues went brown; light multiplied into the LINEAR
albedo is t x f^(1/2.2) on screen, not t x f, flattening bright and dark together; and the palette
grade and the palette's glowing last 32 are Quake's software drawing, not Quake II's. Sample a
few pixels across the panels (PIL) before blaming a texture: the "different floor" was the same
texture under the toe. The classic look is now display-referred (`PostSettings::display`), the
light multiplied into the encoded colour (`linear_to_srgb(albedo) * lit`).
