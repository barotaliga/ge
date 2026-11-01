---
title: Upscale a see-through picture by bleeding its colours out, padding by its edges, and filtering its mask apart
date: 2026-11-01
scope: plugins/classicenhance/pictures.cpp
concepts: [pictures, alpha, bleed, pad, glyph-sheet, conchars, upscale, wrap, density, hud, menu]
---

The texture upscaler wraps (a wall tiles), and both the kernel and the network read colour only.
A 2D picture fed straight in pulls the see-through index's colour (magenta, or black in the font
sheet) into every edge and wraps the left edge into the right. What works: spread the opaque
texels' colours into the see-through ones a ring at a time, clamp-pad by 4 texels so the wrap
only meets copies of the edge, crop, and make the alpha separately (the mask bilinear at the new
size, steepened about 0.5 by scale / 1.25, which keeps a one-texel stroke one texel wide). The
font sheet goes a 8 x 8 cell at a time, or glyphs that fill their cell reach into the next one.
On the reading side, keep the classic sizes in ART pixels and carry a density (pixels a classic
pixel): the HUD's `HudArt::density` already existed for the free art; the menu needed
`ClassicArt::density` and every `img->width` in the views read through `width_of`.
