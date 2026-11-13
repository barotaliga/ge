---
title: A carved letter missing in one size is the other size with its straightest lines doubled
date: 2026-11-13
scope: games/shooters_edge/menu/carved_letters.cpp
concepts: [carved-letters, small-capitals, resize, seam, pieced-letters, alphabet]
---

Letters traced from a font and carved in the pack's colours read as fakes beside cut ones (the
player saw it at once in A E F H M). Before tracing, look for the letter in the strips in the
other size: grow a small capital to a capital by doubling three rows and 15 % of the columns, and
shrink by dropping as many, each picked as the line most like its neighbour (inside a straight
stem or bar), never two side by side and never the three at an edge; the rim, bevel and curves
stay the pack's own (`resize_letter`). Read the strips letter by letter first: every row's word
holds more letters than the menu's own words need (the strips held 31 of 52; 15 were cut). Two
letters whose diagonals lie side by side (Save's A and v) are split by a leaning cut (`Piece::lean`).
Letters in neither size are pieced from the cut ones, never traced from a font: the B an I and the
O's right half cut down to its curves (twice), the X two I's leaned each way and crossed, the Z a
leaned I under the E's bars. Where two pieces overlap the brighter pack px wins, so a face crossing
a rim reads as one surface.
