---
title: A pixel font draws taller than the size asked; space rows by what it draws
date: 2026-10-29
scope: engine/ui/console.cpp
concepts: [font, grid-font, pixel-font, charset, row, line-height, layout]
---

A grid font made from a pixel charset snaps its size to a whole multiple of its grid (16 px for
`terminal`), so at a small screen it draws 16 px glyphs when the layout asked for 11. The console
spaced its rows from the style's size alone and the lines overlapped. Lay out rows from the
font's own `metrics(size)` (ascent plus descent), not from the size passed in, whenever the font
can be a grid font.
