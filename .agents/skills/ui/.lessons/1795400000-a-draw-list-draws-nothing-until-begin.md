---
title: A draw list clips to its size, so one never begun draws nothing
date: 2026-11-22
scope: engine/ui/draw.hpp, engine/ui/debug_sheet.cpp, tests/ui_debug_sheet_test.cpp
concepts: [draw-list, begin, clip, raster, debug-sheet]
---

`DrawList::begin(width, height)` sets the outermost clip. A list that was never begun has a 0 x 0
clip, so every text run and box is clipped away and `render` leaves the image as it was, with no
error. The debug sheet's first build laid out its band correctly and drew no letter in it. Call
`list.begin(image.width, image.height)` before the first command whenever you build a list
yourself rather than receive one from a frame. A test that counts the pixels that differ from the
fill catches this at once.
