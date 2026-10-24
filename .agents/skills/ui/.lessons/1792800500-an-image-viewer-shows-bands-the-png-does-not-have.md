---
title: Banding seen in a lab picture may be the viewer's, not the raster's; read the PNG's pixels
date: 2026-10-23
scope: engine/ui/raster.cpp, engine/lab/labs/ui_menu_lab.cpp
concepts: [gradient, banding, png, screenshot, review]
---

A gradient backdrop looked like a dozen coarse bands with ragged edges when the `menu` lab's PNG
was opened in the session's image reader, which reduces a picture's colours. Decoding the PNG
showed every row one code value apart, as a smooth gradient should be. Before adding dither or
changing the raster to fix banding, read the pixel values out of the PNG (a few lines of Python
with zlib) across the gradient; fix only what the bytes show.
