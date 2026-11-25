---
title: An upscaled glyph sheet snaps to the classic cell, not its own
date: 2026-11-24
scope: engine/ui/charset_font.cpp, engine/ui/font.cpp, engine/ui/font.hpp
concepts: [glyph-sheet, sheet-font, pixel-face, snap-h, ce-pictures, console-size]
---

A pixel face is drawn at whole multiples of its grid (`GridFace::pixel_h`), which is right for a
pack's 8 px glyph sheet but wrong for classic enhance's upscaled one: with 32 px cells its smallest
size is 32 px, so a console asking for 18 px drew its letters twice the classic size, nearest. The
size a face snaps to and the way it samples are two things: `snap_h` snaps to the classic 8 px
multiples while the mask is the area mean of the finer texels. Check a picture-enhancing change by
the console's row height at the same window size, not only by how the letters look.
