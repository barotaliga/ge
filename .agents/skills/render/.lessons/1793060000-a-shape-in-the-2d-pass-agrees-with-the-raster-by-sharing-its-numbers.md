---
title: A shape in the 2D pass agrees with the raster when the CPU works out its numbers once
date: 2026-10-24
scope: engine/ui/draw.cpp, engine/render/ui_pass.wgsl, engine/ui/raster.cpp
concepts: [ui, 2d-pass, parity, sdf, arc, polygon, wgsl, reserved-word]
---

The GPU's arcs, polygons and turned boxes matched the CPU raster on the first try because every
number a shape's distance needs (an arc's end and middle directions and the half sweep's cosine,
a polygon's edge normals, a turn's sine and cosine, the shape's bounds) is worked out once in
the draw list with the engine's libm and handed to both drawers. The shader never calls `sin` or
`cos`, so the two read the same bits and differ only in float rounding. That rounding still
shows at an acute polygon vertex, where coverage swings on the last bit: the `menu-gpu` lab's
needle point is the one pixel a picture that is more than 8 levels off. Judge a shape's parity by
the count of pixels over a few levels, not by the worst pixel alone. Also, `from` is a reserved
word in WGSL; a struct field named `from` fails Tint with an error that does not say so plainly.
