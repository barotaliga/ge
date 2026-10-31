---
title: The glyph sheet's second-set blank is a drawn glyph; skip it when reading text back
date: 2026-10-31
scope: games/shooters_edge/menu/scoreboard.cpp
concepts: [glyph-sheet, conchars, alt, second-set, padded, test, row-text]
---

Text drawn in the classic glyph sheet's second set ORs 128 into every byte, the padding spaces
too, so a right-aligned number in that set (the scoreboard's loss over 25%) draws glyph 160 for
each leading space. `ClassicPen::glyph` skips only 0 and 32. The picture is right (the cell is
blank), but a test that reads a row back from the draw list's boxes sees an extra glyph: leave
code 160 out when turning boxes back into text, and sort the row's glyphs by x, since the
brackets and the name are drawn after the figures.
